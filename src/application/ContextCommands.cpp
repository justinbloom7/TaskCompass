// SPDX-License-Identifier: GPL-3.0-or-later
#include "CommandService.h"
#include "domain/FractionalRank.h"

#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>
#include <tuple>
#include <type_traits>

namespace taskcompass {
namespace {

template<typename Record>
const Record *findRecord(const std::vector<Record> &records, const std::string &id) {
   const auto found = std::ranges::find(records, id, &Record::id);
   return found == records.end() ? nullptr : &*found;
}

template<typename Record>
void putRecord(std::vector<Record> &records, const Record &record) {
   const auto found = std::ranges::find(records, record.id, &Record::id);
   if (found == records.end()) { records.push_back(record); }
   else { *found = record; }
}

bool live(const EntitySnapshot &record) {
   return std::visit([](const auto &value) { return !value.deletedAt; }, record);
}

EntitySnapshot logical(EntitySnapshot record) {
   std::visit([](auto &value) { value.localRevision = 0; value.updatedAt = 0; }, record);
   return record;
}

WorkspacePatch update(std::optional<EntitySnapshot> before, EntitySnapshot after, unsigned fields, Timestamp time) {
   std::visit([&](auto &value) {
      using Record = std::decay_t<decltype(value)>;
      const auto *old = before ? &std::get<Record>(*before) : nullptr;
      if (old && (old->updatedAt == std::numeric_limits<Timestamp>::max()
                  || old->localRevision == std::numeric_limits<std::int64_t>::max())) {
         throw std::runtime_error("Context mutation revision or timestamp exhausted.");
      }
      value.localRevision = old ? old->localRevision + 1 : 1;
      value.updatedAt = std::max({time, value.createdAt, value.deletedAt.value_or(0), old ? old->updatedAt + 1 : 0});
      if constexpr (std::is_same_v<Record, Context>) {
         value.updatedAt = std::max(value.updatedAt, value.archivedAt.value_or(0));
      }
   }, after);
   return {std::move(before), std::move(after), fields};
}

AssignmentIntent assignmentIntent(const WorkspaceSnapshot &before, const WorkspaceSnapshot &after, const std::string &taskId) {
   const auto *sourceLink = primaryContext(before, taskId);
   const auto *destinationLink = primaryContext(after, taskId);
   const auto *source = effectiveContext(before, taskId);
   const auto *destination = effectiveContext(after, taskId);
   const auto optional = [](const auto *record, auto member) -> std::optional<std::string> {
      return record ? std::optional(record->*member) : std::nullopt;
   };
   return {optional(sourceLink, &TaskContext::id), optional(destinationLink, &TaskContext::id),
           optional(sourceLink, &TaskContext::contextId), optional(destinationLink, &TaskContext::contextId),
           optional(source, &Context::id), optional(destination, &Context::id)};
}

struct Plan {
   explicit Plan(const WorkspaceSnapshot &snapshot, Timestamp time) : before(snapshot), after(snapshot), time(time) {}

   const WorkspaceSnapshot &before;
   WorkspaceSnapshot after;
   Timestamp time;
   std::vector<WorkspacePatch> patches;
   std::vector<ContextGuard> guards;

   void change(EntitySnapshot record, unsigned fields) {
      const auto prior = findEntity(before, entityKey(record));
      auto patch = update(prior, std::move(record), fields, time);
      putEntity(after, patch.after);
      patches.push_back(std::move(patch));
   }

   void watch(ContextGuardKind kind, std::string id = {}, std::int64_t priority = 0) {
      if (std::ranges::any_of(guards, [&](const auto &guard) {
         return guard.kind == kind && guard.id == id && guard.priority == priority;
      })) { return; }
      ContextGuard guard{kind, std::move(id), priority, {}, {}, {}, {}};
      guard.before = contextWitness(before, guard);
      guards.push_back(std::move(guard));
   }

   void finalizeGuards() {
      for (auto &guard : guards) {
         guard.after = contextWitness(after, guard);
         if (guard.kind != ContextGuardKind::ORDER && guard.kind != ContextGuardKind::BAND) { continue; }
         const auto common = std::min(guard.before.size(), guard.after.size());
         std::size_t prefix = 0;
         while (prefix < common && guard.before[prefix] == guard.after[prefix]) { ++prefix; }
         std::size_t suffix = 0;
         while (suffix < common - prefix
                && guard.before[guard.before.size() - 1 - suffix] == guard.after[guard.after.size() - 1 - suffix]) {
            ++suffix;
         }
         if (prefix) { guard.left = entityKey(guard.before[prefix - 1]); }
         if (suffix) { guard.right = entityKey(guard.before[guard.before.size() - suffix]); }
         guard.before = contextWitness(before, guard);
         guard.after = contextWitness(after, guard);
      }
   }

   void place(const ContextGuard &band, EntitySnapshot &moving, std::size_t gap) {
      auto rows = contextWitness(before, band);
      std::erase_if(rows, [&](const auto &row) { return entityKey(row) == entityKey(moving); });
      auto rank = [](EntitySnapshot &row) -> std::string & {
         if (auto *context = std::get_if<Context>(&row)) { return context->sortKey; }
         return std::get<Task>(row).manualOrderKey;
      };
      std::vector<std::string> ranks;
      for (auto &row : rows) { ranks.push_back(rank(row)); }
      const auto placement = placeRank(ranks, gap);
      rows.insert(rows.begin() + static_cast<std::ptrdiff_t>(gap), moving);
      for (std::size_t offset = 0; offset < placement.ranks.size(); ++offset) {
         const auto index = placement.first + offset;
         if (index == gap) { rank(moving) = placement.ranks[offset]; }
         else if (rank(rows[index]) != placement.ranks[offset]) {
            rank(rows[index]) = placement.ranks[offset];
            change(rows[index], RANK);
         }
      }
   }
};

}

std::optional<EntitySnapshot> findEntity(const WorkspaceSnapshot &snapshot, const EntityKey &key) {
   switch (key.kind) {
      case EntityKind::TASK:
         if (const auto *record = findRecord(snapshot.tasks, key.id)) { return *record; }
         return {};
      case EntityKind::CONTEXT:
         if (const auto *record = findRecord(snapshot.contexts, key.id)) { return *record; }
         return {};
      case EntityKind::TASK_CONTEXT:
         if (const auto *record = findRecord(snapshot.taskContexts, key.id)) { return *record; }
         return {};
      default:
         for (const auto &record : snapshot.organizations) {
            if (entityKey(asSnapshot(record)) == key) { return asSnapshot(record); }
         }
         return {};
   }
}

void putEntity(WorkspaceSnapshot &snapshot, const EntitySnapshot &entity) {
   if (const auto *task = std::get_if<Task>(&entity)) { putRecord(snapshot.tasks, *task); }
   else if (const auto *context = std::get_if<Context>(&entity)) { putRecord(snapshot.contexts, *context); }
   else if (const auto *link = std::get_if<TaskContext>(&entity)) { putRecord(snapshot.taskContexts, *link); }
   else {
      auto found = std::ranges::find_if(snapshot.organizations, [&](const auto &record) {
         return entityKey(asSnapshot(record)) == entityKey(entity);
      });
      if (found == snapshot.organizations.end()) { snapshot.organizations.push_back(asOrganization(entity)); }
      else { *found = asOrganization(entity); }
   }
}

void validateContextState(const WorkspaceSnapshot &snapshot, const std::string &datasetId) {
   std::set<std::string> assigned;
   for (const auto &context : snapshot.contexts) {
      validateContext(context);
      if (context.datasetId != datasetId) { throw std::invalid_argument("Context belongs to another dataset."); }
   }
   for (const auto &link : snapshot.taskContexts) {
      validateTaskContext(link);
      const auto *context = findRecord(snapshot.contexts, link.contextId);
      const auto *task = findRecord(snapshot.tasks, link.taskId);
      if (link.datasetId != datasetId || !context || !task || task->datasetId != datasetId
          || (!link.deletedAt && (context->deletedAt || task->deletedAt || !assigned.insert(task->id).second))) {
         throw std::invalid_argument("Invalid live or historical context assignment.");
      }
   }
}

const TaskContext *primaryContext(const WorkspaceSnapshot &snapshot, const std::string &taskId) {
   const auto found = std::ranges::find_if(snapshot.taskContexts, [&](const auto &link) {
      return link.taskId == taskId && !link.deletedAt;
   });
   return found == snapshot.taskContexts.end() ? nullptr : &*found;
}

const Context *effectiveContext(const WorkspaceSnapshot &snapshot, const std::string &taskId, bool includeArchived) {
   const auto *link = primaryContext(snapshot, taskId);
   const auto *context = link ? findRecord(snapshot.contexts, link->contextId) : nullptr;
   return context && !context->deletedAt && (includeArchived || !context->archivedAt) ? context : nullptr;
}

std::vector<Context> orderedContexts(const WorkspaceSnapshot &snapshot, bool includeArchived) {
   auto contexts = snapshot.contexts;
   std::erase_if(contexts, [includeArchived](const auto &context) {
      return context.deletedAt || (!includeArchived && context.archivedAt);
   });
   std::ranges::sort(contexts, [](const auto &left, const auto &right) {
      return std::tie(left.sortKey, left.id) < std::tie(right.sortKey, right.id);
   });
   return contexts;
}

std::vector<EntitySnapshot> contextWitness(const WorkspaceSnapshot &snapshot, const ContextGuard &guard) {
   std::vector<EntitySnapshot> result;
   switch (guard.kind) {
      case ContextGuardKind::ORDER:
         for (const auto &context : orderedContexts(snapshot, true)) { result.emplace_back(context); }
         break;
      case ContextGuardKind::LINKS:
      case ContextGuardKind::TASK_LINK:
         for (const auto &link : snapshot.taskContexts) {
            const auto &id = guard.kind == ContextGuardKind::LINKS ? link.contextId : link.taskId;
            if (id == guard.id && !link.deletedAt) { result.emplace_back(link); }
         }
         std::ranges::sort(result, {}, entityKey);
         break;
      case ContextGuardKind::BAND:
         for (const auto &task : snapshot.tasks) {
            if (task.deletedAt || task.priority != guard.priority) { continue; }
            const auto *context = effectiveContext(snapshot, task.id);
            if ((context && context->id == guard.id) || (!context && guard.id.empty())) { result.emplace_back(task); }
         }
         std::ranges::sort(result, [](const auto &left, const auto &right) {
            const auto &a = std::get<Task>(left);
            const auto &b = std::get<Task>(right);
            return std::tie(a.manualOrderKey, a.id) < std::tie(b.manualOrderKey, b.id);
         });
         break;
   }
   const auto start = guard.left ? std::ranges::find(result, *guard.left, entityKey) : result.begin();
   const auto end = guard.right ? std::ranges::find(result, *guard.right, entityKey) : result.end();
   if ((guard.left && start == result.end()) || (guard.right && end == result.end()) || start > end) {
      throw std::runtime_error("Context placement boundary changed.");
   }
   return {start, guard.right ? end + 1 : end};
}

WorkspaceReceipt CommandService::execute(const ContextCommand &command) {
   return executePrepared([&](const WorkspaceSnapshot &snapshot) {
      if (snapshot.schemaVersion != 3) { throw std::runtime_error("Contexts require schema 3."); }
      if (command.expectedWorkspace && *command.expectedWorkspace != workspaceRevisions(snapshot)) {
         throw std::runtime_error("Context preview changed. Reload and review the operation.");
      }
      const auto issuedAt = clock();
      Plan plan(snapshot, issuedAt);
      unsigned fields = 0;
      CommandOperation operation;
      std::optional<EntitySnapshot> before;
      EntitySnapshot after;
      std::vector<RevisionCheck> dependencies;
      if (command.operation == ContextOperation::ASSIGN) {
         before = requireEntity(snapshot, {EntityKind::TASK, command.id}, command.expectedRevision);
         after = *before;
         const auto &task = std::get<Task>(after);
         if (task.deletedAt) { throw std::invalid_argument("Task is removed."); }
         OrganizationRules(datasetId, snapshot.organizations).requireTaskOwner(task, false);
         const auto *old = primaryContext(snapshot, task.id);
         const auto *effective = effectiveContext(snapshot, task.id);
         const std::string destination = command.destination.value_or("");
         if ((!command.destination && !old) || (command.destination && old && old->contextId == destination)) {
            throw std::invalid_argument("Task already has that assignment.");
         }
         plan.watch(ContextGuardKind::TASK_LINK, task.id);
         if (old) {
            auto removed = *old;
            removed.deletedAt = std::max(issuedAt, removed.updatedAt);
            plan.change(removed, REMOVAL);
            const auto *context = findRecord(snapshot.contexts, old->contextId);
            dependencies.push_back({entityKey(*context), context->localRevision});
         }
         if (command.destination) {
            const auto *context = findRecord(snapshot.contexts, destination);
            if (!context || context->deletedAt || context->archivedAt) { throw std::invalid_argument("Choose an active context."); }
            dependencies.push_back({entityKey(*context), context->localRevision});
            const auto linkId = generateId();
            if (findRecord(snapshot.taskContexts, linkId)) { throw std::runtime_error("Duplicate assignment identity."); }
            plan.change(TaskContext{linkId, datasetId, task.id, destination, issuedAt, 0, {}, 0}, REMOVAL);
         }
         plan.watch(ContextGuardKind::BAND, effective ? effective->id : "", task.priority);
         plan.watch(ContextGuardKind::BAND, destination, task.priority);
         if ((effective ? effective->id : "") != destination) {
            const ContextGuard band{ContextGuardKind::BAND, destination, task.priority, {}, {}, {}, {}};
            plan.place(band, after, contextWitness(snapshot, band).size());
            fields |= RANK;
         }
         operation = CommandOperation::ASSIGN;
      } else {
         const auto order = orderedContexts(snapshot, true);
         if (command.operation == ContextOperation::CREATE) {
            after = Context{generateId(), datasetId, normalizeTitle(command.name), {}, issuedAt, 0, {}, {}, 0};
            plan.watch(ContextGuardKind::ORDER);
            plan.place(plan.guards.front(), after, order.size());
            operation = CommandOperation::CREATE;
            fields = REMOVAL;
         } else {
            before = requireEntity(snapshot, {EntityKind::CONTEXT, command.id}, command.expectedRevision);
            after = *before;
            auto &context = std::get<Context>(after);
            if (context.deletedAt) { throw std::invalid_argument("Context is deleted. Use Undo to restore it."); }
            switch (command.operation) {
               case ContextOperation::RENAME:
                  context.name = normalizeTitle(command.name);
                  operation = CommandOperation::EDIT;
                  fields = TITLE;
                  break;
               case ContextOperation::ARCHIVE:
                  if (context.archivedAt) { throw std::invalid_argument("Context is already archived."); }
                  context.archivedAt = std::max(issuedAt, context.updatedAt);
                  plan.watch(ContextGuardKind::LINKS, context.id);
                  operation = CommandOperation::ARCHIVE;
                  fields = ARCHIVE;
                  break;
               case ContextOperation::RESTORE:
                  if (!context.archivedAt) { throw std::invalid_argument("Context is already active."); }
                  context.archivedAt.reset();
                  plan.watch(ContextGuardKind::LINKS, context.id);
                  operation = CommandOperation::RESTORE;
                  fields = ARCHIVE;
                  break;
               case ContextOperation::DELETE_CONTEXT: {
                  plan.watch(ContextGuardKind::ORDER);
                  plan.watch(ContextGuardKind::LINKS, context.id);
                  std::vector<RevisionCheck> impact;
                  for (const auto &record : plan.guards.back().before) { impact.push_back({entityKey(record), entityRevision(record)}); }
                  if (!command.expectedLinks || *command.expectedLinks != impact) {
                     throw std::invalid_argument("Context deletion requires confirmation of the current affected assignments.");
                  }
                  for (const auto &record : plan.guards.back().before) {
                     auto link = std::get<TaskContext>(record);
                     link.deletedAt = std::max(issuedAt, link.updatedAt);
                     plan.change(link, REMOVAL);
                  }
                  context.deletedAt = std::max(issuedAt, context.updatedAt);
                  operation = CommandOperation::DELETE_CONTEXT;
                  fields = REMOVAL;
                  break;
               }
               case ContextOperation::MOVE_UP:
               case ContextOperation::MOVE_DOWN: {
                  const auto index = static_cast<std::size_t>(std::ranges::find(order, context.id, &Context::id) - order.begin());
                  const bool up = command.operation == ContextOperation::MOVE_UP;
                  if (!command.expectedWorkspace || (up ? index == 0 : index + 1 == order.size())) {
                     throw std::invalid_argument("Context reorder requires a current destination.");
                  }
                  plan.watch(ContextGuardKind::ORDER);
                  plan.place(plan.guards.front(), after, up ? index - 1 : index + 1);
                  operation = CommandOperation::REORDER;
                  fields = RANK;
                  break;
               }
               default: throw std::invalid_argument("Invalid context operation.");
            }
         }
      }
      const auto key = entityKey(after);
      if (!before && findEntity(snapshot, key)) { throw std::runtime_error("Duplicate context identity."); }
      auto result = receipt(operation, before, after, fields, snapshot);
      result.issuedAt = issuedAt;
      for (const auto &dependency : dependencies) { result.dependencies.push_back(dependency); }
      validateChange(snapshot, result.after, command.operation == ContextOperation::ASSIGN);
      putEntity(plan.after, result.after);
      validateContextState(plan.after, datasetId);
      plan.finalizeGuards();
      if (command.operation == ContextOperation::ASSIGN) {
         result.assignment = assignmentIntent(snapshot, plan.after, command.id);
      }
      result.patches = std::move(plan.patches);
      result.contextGuards = std::move(plan.guards);
      return result;
   });
}

WorkspaceReceipt CommandService::execute(const TaskPriorityCommand &command) {
   return executePrepared([&](const WorkspaceSnapshot &snapshot) {
      if (snapshot.schemaVersion != 3) { throw std::runtime_error("Task priorities require schema 3."); }
      const auto before = requireEntity(snapshot, {EntityKind::TASK, command.taskId}, command.expectedRevision);
      auto task = std::get<Task>(before);
      if (task.deletedAt) { throw std::invalid_argument("Task is removed."); }
      if (command.priority <= 0 || task.priority == command.priority) {
         throw std::invalid_argument("Choose a different positive task priority.");
      }
      OrganizationRules(datasetId, snapshot.organizations).requireTaskOwner(task, false);
      const auto issuedAt = clock();
      const auto *context = effectiveContext(snapshot, task.id);
      const std::string group = context ? context->id : "";
      Plan plan(snapshot, issuedAt);
      plan.watch(ContextGuardKind::TASK_LINK, task.id);
      plan.watch(ContextGuardKind::BAND, group, task.priority);
      plan.watch(ContextGuardKind::BAND, group, command.priority);
      task.priority = command.priority;
      EntitySnapshot after = task;
      const auto &destination = plan.guards.back();
      plan.place(destination, after, destination.before.size());
      auto result = receipt(CommandOperation::EDIT, before, std::move(after), PRIORITY | RANK, snapshot);
      result.issuedAt = issuedAt;
      if (const auto *link = primaryContext(snapshot, task.id)) {
         const auto *assigned = findRecord(snapshot.contexts, link->contextId);
         result.dependencies.push_back({entityKey(*assigned), assigned->localRevision});
      }
      validateChange(snapshot, result.after, false);
      putEntity(plan.after, result.after);
      plan.finalizeGuards();
      result.contextGuards = std::move(plan.guards);
      result.patches = std::move(plan.patches);
      return result;
   });
}

void CommandService::prepareTaskCaptureRank(WorkspaceReceipt &result, const WorkspaceSnapshot &snapshot) {
   Plan plan(snapshot, result.issuedAt);
   plan.watch(ContextGuardKind::BAND, {}, std::get<Task>(result.after).priority);
   auto &guard = plan.guards.front();
   plan.place(guard, result.after, guard.before.size());
   putEntity(plan.after, result.after);
   plan.finalizeGuards();
   result.contextGuards = std::move(plan.guards);
   result.patches = std::move(plan.patches);
}

WorkspaceReceipt CommandService::compensateContext(const WorkspaceReceipt &original, const WorkspaceSnapshot &snapshot,
                                                    bool forward, const std::string &lastCommandId) {
   const auto issuedAt = clock();
   Plan plan(snapshot, issuedAt);
   const auto conflict = []() { throw std::runtime_error("Context operation is stale. Reload before retrying undo or redo."); };
   const auto verify = [&](const EntitySnapshot &current, const EntitySnapshot &expected) {
      const auto key = entityKey(expected);
      const auto known = knownRevisions.find(key);
      if (logical(current) != logical(expected)
          || entityRevision(current) != (known == knownRevisions.end() ? entityRevision(expected) : known->second)) { conflict(); }
   };
   for (const auto &guard : original.contextGuards) {
      const auto actual = contextWitness(snapshot, guard);
      const auto &expected = forward ? guard.before : guard.after;
      if (actual.size() != expected.size()) { conflict(); }
      for (std::size_t i = 0; i < actual.size(); ++i) { verify(actual[i], expected[i]); }
   }
   for (const auto &dependency : original.dependencies) {
      const auto known = knownRevisions.find(dependency.key);
      requireEntity(snapshot, dependency.key, known == knownRevisions.end() ? dependency.revision : known->second);
   }
   const auto inverse = [&](const WorkspacePatch &patch) {
      const auto key = entityKey(patch.after);
      const auto actual = findEntity(snapshot, key);
      const auto &expected = forward ? patch.before : std::optional<EntitySnapshot>(patch.after);
      if (!actual) { conflict(); }
      if (expected) { verify(*actual, *expected); }
      else if (live(*actual) || !knownRevisions.contains(key) || knownRevisions.at(key) != entityRevision(*actual)) { conflict(); }
      auto desired = forward ? patch.after : patch.before.value_or(patch.after);
      if (!forward && !patch.before) {
         std::visit([&](auto &value) { value.deletedAt = std::max(issuedAt, value.updatedAt); }, desired);
      }
      return update(actual, std::move(desired), patch.changedFields, issuedAt);
   };
   const auto root = inverse({original.before, original.after, original.changedFields});
   auto result = receipt(forward ? CommandOperation::REDO : CommandOperation::UNDO,
                         root.before, root.after, root.changedFields, snapshot);
   result.issuedAt = issuedAt;
   result.dependencies = original.dependencies;
   for (auto &dependency : result.dependencies) {
      dependency.revision = entityRevision(*findEntity(snapshot, dependency.key));
   }
   putEntity(plan.after, result.after);
   for (const auto &patch : original.patches) {
      auto changed = inverse(patch);
      putEntity(plan.after, changed.after);
      result.patches.push_back(std::move(changed));
   }
   validateContextState(plan.after, datasetId);
   validateChange(snapshot, result.after, true);
   for (const auto &guard : original.contextGuards) {
      auto next = guard;
      next.before = contextWitness(snapshot, guard);
      next.after = contextWitness(plan.after, guard);
      const auto &desired = forward ? guard.after : guard.before;
      if (next.after.size() != desired.size()) { conflict(); }
      for (std::size_t i = 0; i < desired.size(); ++i) {
         if (logical(next.after[i]) != logical(desired[i])) { conflict(); }
      }
      result.contextGuards.push_back(std::move(next));
   }
   if (original.assignment) {
      result.assignment = assignmentIntent(snapshot, plan.after, entityKey(result.after).id);
   }
   if (forward) { result.reappliesCommandId = original.id; }
   else { result.compensatesCommandId = lastCommandId; }
   return result;
}

}
