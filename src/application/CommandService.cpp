#include "CommandService.h"

#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>
#include <type_traits>

namespace taskcompass {

EntityKey entityKey(const EntitySnapshot &entity) {
   return {static_cast<EntityKind>(entity.index()), std::visit([](const auto &value) { return value.id; }, entity)};
}
OrganizationEntity asOrganization(const EntitySnapshot &entity) {
   return std::visit([](const auto &value) -> OrganizationEntity {
      if constexpr (std::is_constructible_v<OrganizationEntity, decltype(value)>) { return value; }
      else { throw std::invalid_argument("Expected organization."); }
   }, entity);
}
EntitySnapshot asSnapshot(const OrganizationEntity &entity) {
   return std::visit([](const auto &value) -> EntitySnapshot { return value; }, entity);
}
std::int64_t entityRevision(const EntitySnapshot &entity) {
   return std::visit([](const auto &value) { return value.localRevision; }, entity);
}
std::string entityDataset(const EntitySnapshot &entity) {
   return std::visit([](const auto &value) { return value.datasetId; }, entity);
}
std::vector<RevisionCheck> workspaceRevisions(const WorkspaceSnapshot &snapshot) {
   std::vector<RevisionCheck> result;
   for (const auto &task : snapshot.tasks) { result.push_back({entityKey(task), task.localRevision}); }
   for (const auto &entity : snapshot.organizations) { result.push_back({entityKey(asSnapshot(entity)), organizationFields(entity).localRevision}); }
   for (const auto &context : snapshot.contexts) { result.push_back({entityKey(context), context.localRevision}); }
   for (const auto &link : snapshot.taskContexts) { result.push_back({entityKey(link), link.localRevision}); }
   std::sort(result.begin(), result.end(), [](const auto &first, const auto &second) { return first.key < second.key; });
   return result;
}
namespace {
bool removed(const EntitySnapshot &entity) {
   return std::visit([](const auto &value) { return value.deletedAt.has_value(); }, entity);
}
std::string operationDescription(CommandOperation operation) {
   switch (operation) {
      case CommandOperation::CREATE: return "Create";
      case CommandOperation::EDIT: return "Edit";
      case CommandOperation::COMPLETE: return "Complete";
      case CommandOperation::REOPEN: return "Reopen";
      case CommandOperation::MOVE: return "Move";
      case CommandOperation::ARCHIVE: return "Archive";
      case CommandOperation::RESTORE: return "Restore";
      case CommandOperation::DELETE_CONTEXT: return "Delete";
      case CommandOperation::ASSIGN: return "Assign context to";
      case CommandOperation::REORDER: return "Reorder";
      default: return "";
   }
}
std::string label(const WorkspaceReceipt &receipt) {
   if (receipt.changedFields & PRIORITY) { return "Set task priority"; }
   static constexpr const char *NAMES[] = {"task", "pillar", "project", "objective", "context", "assignment"};
   return operationDescription(receipt.operation) + " " + NAMES[receipt.after.index()];
}
}

CommandService::CommandService(WorkspaceRepository &repository, std::string datasetId,
                               Clock clock, IdGenerator generateId)
   : repository(repository), datasetId(std::move(datasetId)), clock(std::move(clock)), generateId(std::move(generateId)) {
   if (!isUuid(this->datasetId)) { throw std::invalid_argument("Invalid workspace dataset."); }
}
WorkspaceSnapshot CommandService::read() { return repository.read(); }
EntitySnapshot CommandService::requireEntity(const WorkspaceSnapshot &snapshot, const EntityKey &key, std::int64_t revision) const {
   const auto found = findEntity(snapshot, key);
   if (!found || entityDataset(*found) != datasetId || entityRevision(*found) != revision) {
      throw std::runtime_error("Record changed or is unavailable. Refresh before retrying.");
   }
   return *found;
}
void CommandService::validateChange(const WorkspaceSnapshot &snapshot, const EntitySnapshot &after, bool allowArchived) const {
   if (entityDataset(after) != datasetId) { throw std::invalid_argument("Cross-dataset mutation."); }
   if (const auto *task = std::get_if<Task>(&after)) {
      validateTask(*task);
      if (!task->deletedAt) { OrganizationRules(datasetId, snapshot.organizations).requireTaskOwner(*task, allowArchived); }
   } else if (const auto *context = std::get_if<Context>(&after)) {
      validateContext(*context);
   } else {
      auto entities = snapshot.organizations;
      const auto value = asOrganization(after);
      const auto key = organizationKey(value);
      auto found = std::find_if(entities.begin(), entities.end(), [&](const auto &entity) { return organizationKey(entity) == key; });
      if (found == entities.end()) { entities.push_back(value); } else { *found = value; }
      OrganizationRules graph(datasetId, entities);
      if (!removed(after)) { graph.requireAvailableParent(value, allowArchived); }
   }
}
WorkspaceReceipt CommandService::receipt(CommandOperation operation, const std::optional<EntitySnapshot> &before,
                                          EntitySnapshot after, unsigned changedFields, const WorkspaceSnapshot &snapshot) {
   std::int64_t previousRevision = 0;
   Timestamp previousTime = 0;
   if (before) {
      previousRevision = entityRevision(*before);
      previousTime = std::visit([](const auto &value) { return value.updatedAt; }, *before);
      if (previousRevision == std::numeric_limits<std::int64_t>::max() || previousTime == std::numeric_limits<Timestamp>::max()) {
         throw std::runtime_error("Record revision or timestamp exhausted.");
      }
   }
   std::visit([&](auto &value) {
      value.localRevision = previousRevision + 1;
      value.updatedAt = std::max({clock(), previousTime + 1, value.createdAt, value.deletedAt.value_or(0)});
      if constexpr (std::is_same_v<std::decay_t<decltype(value)>, Task>) {
         value.updatedAt = std::max(value.updatedAt, value.completedAt.value_or(0));
      } else if constexpr (requires { value.archivedAt; }) {
         value.updatedAt = std::max(value.updatedAt, value.archivedAt.value_or(0));
      }
   }, after);
   WorkspaceReceipt result{generateId(), generateId(), operation, before, std::move(after), changedFields, {}, {}, {}, {}, {}, clock()};
   if (!isUuid(result.id) || !isUuid(result.operationId)) { throw std::runtime_error("ID generator returned invalid UUID."); }
   OrganizationRules graph(datasetId, snapshot.organizations);
   std::set<OrganizationKey> parents;
   auto includeParents = [&](const EntitySnapshot &entity) {
      std::optional<OrganizationKey> parent;
      if (const auto *task = std::get_if<Task>(&entity)) {
         if (task->owner.kind != TaskOwnerKind::INDEPENDENT) {
            parent = {task->owner.kind == TaskOwnerKind::OBJECTIVE ? OrganizationKind::OBJECTIVE : OrganizationKind::PROJECT, task->owner.id};
         }
      } else if (entity.index() < static_cast<std::size_t>(EntityKind::CONTEXT)) {
         parent = organizationParent(asOrganization(entity));
      }
      if (parent) {
         for (const auto &ancestor : graph.path(*parent)) { parents.insert(ancestor); }
      }
   };
   if (before) { includeParents(*before); }
   includeParents(result.after);
   for (const auto &key : parents) {
      const auto &entity = graph.find(key);
      result.dependencies.push_back({entityKey(asSnapshot(entity)), organizationFields(entity).localRevision});
   }
   return result;
}
WorkspaceReceipt CommandService::executePrepared(const WorkspaceRepository::Prepare &prepare) {
   WorkspaceReceipt result;
   try { result = repository.transact(prepare); }
   catch (const CommitOutcomeUnknown &) { clearHistory(); throw; }
   const auto mismatched = [&](const EntitySnapshot &record) {
      const auto known = knownRevisions.find(entityKey(record));
      return known != knownRevisions.end() && known->second != entityRevision(record);
   };
   bool external = result.before && mismatched(*result.before);
   for (const auto &patch : result.patches) { external = external || (patch.before && mismatched(*patch.before)); }
   for (const auto &guard : result.contextGuards) {
      external = external || std::ranges::any_of(guard.before, mismatched);
   }
   for (const auto &dependency : result.dependencies) {
      external = external || (knownRevisions.contains(dependency.key) && knownRevisions.at(dependency.key) != dependency.revision);
   }
   if (external) { clearHistory(); }
   history.erase(history.begin() + static_cast<std::ptrdiff_t>(historyIndex), history.end());
   history.push_back({result, result.id});
   if (history.size() > HISTORY_LIMIT) { history.erase(history.begin()); }
   historyIndex = history.size();
   remember(result);
   return result;
}

void CommandService::remember(const WorkspaceReceipt &result) {
   for (const auto &dependency : result.dependencies) { knownRevisions.try_emplace(dependency.key, dependency.revision); }
   for (const auto &guard : result.contextGuards) {
      for (const auto &record : guard.before) { knownRevisions.try_emplace(entityKey(record), entityRevision(record)); }
   }
   knownRevisions[entityKey(result.after)] = entityRevision(result.after);
   for (const auto &patch : result.patches) { knownRevisions[entityKey(patch.after)] = entityRevision(patch.after); }
}

WorkspaceReceipt CommandService::execute(const TaskCommand &command) {
   return executePrepared([&](const WorkspaceSnapshot &snapshot) {
      if (command.expectedWorkspace && *command.expectedWorkspace != workspaceRevisions(snapshot)) {
         throw std::runtime_error("Task owner preview changed. Reload and confirm the current destination.");
      }
      if (command.ownerChange && command.allowArchivedDestination && !command.expectedWorkspace) {
         throw std::invalid_argument("Archived destination confirmation requires a current preview.");
      }
      std::optional<EntitySnapshot> before;
      Task after;
      unsigned fields = 0;
      if (command.operation == TaskOperation::CREATE) {
         after.id = generateId(); after.datasetId = datasetId; after.createdAt = clock();
         after.title = normalizeTitle(command.title); after.notes = command.notes;
         after.owner = command.ownerChange.value_or(TaskOwner{});
         fields = REMOVAL;
      } else {
         before = requireEntity(snapshot, {EntityKind::TASK, command.taskId}, command.expectedRevision);
         after = std::get<Task>(*before);
         if (after.deletedAt) { throw std::runtime_error("Task is removed."); }
         switch (command.operation) {
            case TaskOperation::EDIT:
               after.title = normalizeTitle(command.title); after.notes = command.notes;
               fields = TITLE | NOTES;
               if (command.ownerChange) { after.owner = *command.ownerChange; fields |= OWNER; }
               break;
            case TaskOperation::COMPLETE:
               if (after.status != TaskStatus::ACTIVE) { throw std::invalid_argument("Only active tasks can be completed."); }
               if (after.updatedAt == std::numeric_limits<Timestamp>::max()) { throw std::runtime_error("Timestamp exhausted."); }
               after.status = TaskStatus::COMPLETED; after.completedAt = std::max(clock(), after.updatedAt + 1); fields = LIFECYCLE;
               break;
            case TaskOperation::REOPEN:
               if (after.status != TaskStatus::COMPLETED) { throw std::invalid_argument("Only completed tasks can be reopened."); }
               after.status = TaskStatus::ACTIVE; after.completedAt.reset(); fields = LIFECYCLE;
               break;
            default: throw std::invalid_argument("Use session history for undo/redo.");
         }
      }
      auto result = receipt(static_cast<CommandOperation>(command.operation), before, after, fields, snapshot);
      if (command.operation == TaskOperation::CREATE && snapshot.schemaVersion == 3) {
         prepareTaskCaptureRank(result, snapshot);
      }
      const bool unchangedOwner = before && std::get<Task>(*before).owner == std::get<Task>(result.after).owner;
      validateChange(snapshot, result.after, unchangedOwner || (command.operation != TaskOperation::CREATE && command.allowArchivedDestination));
      return result;
   });
}
WorkspaceReceipt CommandService::execute(const OrganizationCommand &command) {
   return executePrepared([&](const WorkspaceSnapshot &snapshot) {
      if (command.expectedWorkspace && *command.expectedWorkspace != workspaceRevisions(snapshot)) {
         throw std::runtime_error("Organization preview changed. Reload and confirm the current impact.");
      }
      std::optional<EntitySnapshot> before;
      OrganizationEntity after;
      unsigned fields = 0;
      CommandOperation operation = CommandOperation::CREATE;
      if (command.operation == OrganizationOperation::CREATE) {
         switch (command.kind) {
            case OrganizationKind::PILLAR: after = Pillar{}; break;
            case OrganizationKind::PROJECT: after = Project{}; break;
            case OrganizationKind::OBJECTIVE: after = Objective{}; break;
            default: throw std::invalid_argument("Unknown organization kind.");
         }
         auto &value = organizationFields(after);
         value.id = generateId(); value.datasetId = datasetId; value.createdAt = clock();
         setOrganizationTitle(after, command.title); value.description = command.description;
         setOrganizationParent(after, command.parent); fields = REMOVAL;
      } else {
         before = requireEntity(snapshot, {static_cast<EntityKind>(static_cast<int>(command.kind) + 1), command.id}, command.expectedRevision);
         after = asOrganization(*before);
         auto &value = organizationFields(after);
         if (value.deletedAt) { throw std::runtime_error("Organization is removed."); }
         switch (command.operation) {
            case OrganizationOperation::EDIT:
               setOrganizationTitle(after, command.title); value.description = command.description;
               operation = CommandOperation::EDIT; fields = TITLE | NOTES; break;
            case OrganizationOperation::MOVE:
               setOrganizationParent(after, command.parent); operation = CommandOperation::MOVE; fields = OWNER; break;
            case OrganizationOperation::ARCHIVE:
               if (value.archivedAt) { throw std::invalid_argument("Already explicitly archived."); }
               if (value.updatedAt == std::numeric_limits<Timestamp>::max()) { throw std::runtime_error("Timestamp exhausted."); }
               value.archivedAt = std::max(clock(), value.updatedAt + 1); operation = CommandOperation::ARCHIVE; fields = ARCHIVE; break;
            case OrganizationOperation::RESTORE:
               if (!value.archivedAt) { throw std::invalid_argument("No explicit archive flag; restore the archived ancestor instead."); }
               value.archivedAt.reset(); operation = CommandOperation::RESTORE; fields = ARCHIVE; break;
            default: throw std::invalid_argument("Unknown organization operation.");
         }
      }
      auto result = receipt(operation, before, asSnapshot(after), fields, snapshot);
      const bool move = command.operation == OrganizationOperation::MOVE;
      if (move && command.allowArchivedDestination && !command.expectedWorkspace) {
         throw std::invalid_argument("Archived destination confirmation must include a current preview.");
      }
      validateChange(snapshot, result.after, command.operation != OrganizationOperation::CREATE && (!move || command.allowArchivedDestination));
      return result;
   });
}
WorkspaceReceipt CommandService::compensate(bool forward) {
   if (forward ? !canRedo() : !canUndo()) { throw std::runtime_error("No command available in session history."); }
   auto &entry = history[forward ? historyIndex : historyIndex - 1];
   const auto original = entry.original;
   WorkspaceReceipt result;
   try {
      result = repository.transact([&](const WorkspaceSnapshot &snapshot) {
         if (!original.contextGuards.empty() || !original.patches.empty() || std::holds_alternative<Context>(original.after)) {
            return compensateContext(original, snapshot, forward, entry.lastCommandId);
         }
         const auto key = entityKey(original.after);
         const auto current = requireEntity(snapshot, key, knownRevisions.at(key));
         for (const auto &dependency : original.dependencies) {
            const auto revision = knownRevisions.contains(dependency.key) ? knownRevisions.at(dependency.key) : dependency.revision;
            requireEntity(snapshot, dependency.key, revision);
         }
         auto after = current;
         const auto &target = forward ? original.after : original.before.value_or(original.after);
         std::visit([&](auto &value) {
            using Value = std::decay_t<decltype(value)>;
            const auto &desired = std::get<Value>(target);
            if (original.operation == CommandOperation::CREATE) {
               if (!forward && key.kind != EntityKind::TASK) {
                  OrganizationRules(datasetId, snapshot.organizations).requireRemovable(organizationKey(asOrganization(after)), snapshot.tasks);
               }
               if (value.updatedAt == std::numeric_limits<Timestamp>::max()) { throw std::runtime_error("Timestamp exhausted."); }
               value.deletedAt = forward ? std::nullopt : std::optional<Timestamp>(std::max(clock(), value.updatedAt + 1));
            } else if constexpr (std::is_same_v<Value, Task>) {
               if (original.changedFields & TITLE) { value.title = desired.title; }
               if (original.changedFields & NOTES) { value.notes = desired.notes; }
               if (original.changedFields & OWNER) { value.owner = desired.owner; }
               if (original.changedFields & LIFECYCLE) { value.status = desired.status; value.completedAt = desired.completedAt; }
            } else if constexpr (std::is_constructible_v<OrganizationEntity, Value>) {
               if (original.changedFields & TITLE) {
                  if constexpr (std::is_same_v<Value, Pillar>) { value.name = desired.name; }
                  else { value.title = desired.title; }
               }
               if (original.changedFields & NOTES) { value.description = desired.description; }
               if (original.changedFields & ARCHIVE) { value.archivedAt = desired.archivedAt; }
               if (original.changedFields & OWNER) {
                  OrganizationEntity changed = value;
                  setOrganizationParent(changed, organizationParent(OrganizationEntity(desired)));
                  value = std::get<Value>(changed);
               }
            }
         }, after);
         auto inverse = receipt(forward ? CommandOperation::REDO : CommandOperation::UNDO, current, after, original.changedFields, snapshot);
         validateChange(snapshot, inverse.after, true);
         if (forward) { inverse.reappliesCommandId = original.id; }
         else { inverse.compensatesCommandId = entry.lastCommandId; }
         return inverse;
      });
   } catch (const CommitOutcomeUnknown &) { clearHistory(); throw; }
   entry.lastCommandId = result.id;
   if (forward) { ++historyIndex; } else { --historyIndex; }
   remember(result);
   return result;
}
WorkspaceReceipt CommandService::undo() { return compensate(false); }
WorkspaceReceipt CommandService::redo() { return compensate(true); }
bool CommandService::canUndo() const { return historyIndex > 0; }
bool CommandService::canRedo() const { return historyIndex < history.size(); }
std::string CommandService::undoDescription() const { return canUndo() ? label(history[historyIndex - 1].original) : ""; }
std::string CommandService::redoDescription() const { return canRedo() ? label(history[historyIndex].original) : ""; }
void CommandService::clearHistory() { history.clear(); knownRevisions.clear(); historyIndex = 0; }

}
