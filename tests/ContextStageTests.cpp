// SPDX-License-Identifier: GPL-3.0-or-later
#include "MemoryWorkspaceRepository.h"
#include "application/CommandService.h"
#include "desktop/persistent/PersistentTaskFilter.h"
#include "desktop/persistent/PersistentTaskModel.h"
#include "domain/FractionalRank.h"
#include "infrastructure/storage/MigrationRunner.h"
#include "infrastructure/storage/DatabaseConfig.h"
#include "infrastructure/storage/WorkspaceCodec.h"

#include <QApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMimeData>
#include <QProcess>
#include <QProcessEnvironment>
#include <iostream>
#include <limits>
#include <source_location>
#include <stdexcept>

using namespace taskcompass;

namespace {

void check(bool condition, std::source_location here = std::source_location::current()) {
   if (!condition) { throw std::runtime_error("Assertion failed at line " + std::to_string(here.line())); }
}
template<typename Run>
void rejects(const Run &run) {
   try { run(); }
   catch (const std::exception &) { return; }
   throw std::runtime_error("Expected the operation to be rejected.");
}

std::string id(int value) {
   auto digits = std::to_string(value);
   return "12345678-0000-0000-0000-" + std::string(12 - digits.size(), '0') + digits;
}

void rankTests() {
   const auto a = std::string(126, '0') + '1';
   const auto b = std::string(127, '0') + '1';
   const auto c = std::string(128, '0') + '1';
   check(validRank(a) && validRank(b) && !validRank(c));
   check(!validRank(a + '0') && !validRank(b + '0') && !validRank(std::string(127, '0') + char(0x80)));
   check(validRank(a + '1') && b < a && a < a + '1');
   check(placeRank({a}, 0).ranks == std::vector{b});
   check(placeRank({b, a + '1'}, 1).ranks == std::vector{a});
   check(placeRank({b}, 0).ranks == (std::vector<std::string>{"01", "1"}));
   check(placeRank({"1", "1"}, 1).ranks == (std::vector<std::string>{"01", "1", "11"}));
   check(placeRank({}, 0).ranks == (std::vector<std::string>{"1"}));
   check(placeRank({"01", "1"}, 1).ranks == (std::vector<std::string>{"011"}));
   check(placeRank({"1"}, 1).ranks == (std::vector<std::string>{"11"}));
   check(seedRank(1, 4) == "001" && seedRank(4, 4) == "1");
   check(seedRank(std::numeric_limits<std::uint64_t>::max(), std::numeric_limits<std::uint64_t>::max()) == std::string(64, '1'));
   rejects([] { placeRank(std::vector<std::string>(131, "1"), 65); });
   rejects([] { placeRank({"10"}, 0); });
   rejects([] { placeRank({"1", "01"}, 1); });
}

struct Fixture {
   MemoryWorkspaceRepository repo;
   Timestamp now = 1000;
   int nextId = 100;
   CommandService commands{repo, id(1), [this] { return ++now; }, [this] { return id(++nextId); }};

   Fixture() { repo.snapshot.schemaVersion = 3; }

   Task capture(const std::string &title) {
      TaskCommand command;
      command.operation = TaskOperation::CREATE;
      command.title = title;
      return std::get<Task>(commands.execute(command).after);
   }

   Context create(const std::string &name) {
      ContextCommand command;
      command.name = name;
      return std::get<Context>(commands.execute(command).after);
   }

   WorkspaceReceipt context(ContextOperation operation, const std::string &target, const std::string &name = {}) {
      ContextCommand command;
      command.operation = operation;
      command.id = target;
      command.expectedRevision = entityRevision(*findEntity(repo.snapshot, {EntityKind::CONTEXT, target}));
      command.name = name;
      command.expectedWorkspace = workspaceRevisions(repo.snapshot);
      if (operation == ContextOperation::DELETE_CONTEXT) {
         command.expectedLinks.emplace();
         for (const auto &item : contextWitness(repo.snapshot, {ContextGuardKind::LINKS, target, 0, {}, {}, {}, {}})) {
            command.expectedLinks->push_back({entityKey(item), entityRevision(item)});
         }
      }
      return commands.execute(command);
   }

   WorkspaceReceipt assign(const std::string &task, std::optional<std::string> context) {
      ContextCommand command;
      command.operation = ContextOperation::ASSIGN;
      command.id = task;
      command.expectedRevision = entityRevision(*findEntity(repo.snapshot, {EntityKind::TASK, task}));
      command.destination = std::move(context);
      return commands.execute(command);
   }
};

void contextTests() {
   Fixture f;
   const auto independent = f.capture("A");
   const auto task = f.capture("B");
   check(independent.manualOrderKey == "1" && task.manualOrderKey == "11" && task.priority == 5);
   const auto home = f.create("Home");
   const auto office = f.create("Office");
   f.context(ContextOperation::MOVE_UP, office.id);
   check(orderedContexts(f.repo.snapshot, true).front().id == office.id);
   f.commands.undo();
   check(orderedContexts(f.repo.snapshot, true).front().id == home.id);
   f.commands.redo();
   check(orderedContexts(f.repo.snapshot, true).front().id == office.id);
   f.context(ContextOperation::RENAME, home.id, "  Home office  ");
   check(std::get<Context>(*findEntity(f.repo.snapshot, entityKey(home))).name == "Home office");

   const auto assigned = f.assign(task.id, home.id);
   const auto initialLink = *primaryContext(f.repo.snapshot, task.id);
   check(initialLink.contextId == home.id && assigned.patches.size() == 1);
   f.commands.undo();
   check(!primaryContext(f.repo.snapshot, task.id));
   f.commands.redo();
   check(primaryContext(f.repo.snapshot, task.id)->id == initialLink.id);
   f.context(ContextOperation::ARCHIVE, home.id);
   check(!effectiveContext(f.repo.snapshot, task.id));
   check(effectiveContext(f.repo.snapshot, task.id, true)->id == home.id);
   rejects([&] { f.assign(independent.id, home.id); });

   const auto clearArchived = f.assign(task.id, {});
   check(clearArchived.assignment && clearArchived.assignment->sourceContextId == home.id
         && !clearArchived.assignment->effectiveSourceId && !clearArchived.assignment->destinationContextId);
   check(!primaryContext(f.repo.snapshot, task.id));
   f.commands.undo();
   check(primaryContext(f.repo.snapshot, task.id)->id == initialLink.id);
   f.context(ContextOperation::RESTORE, home.id);
   check(effectiveContext(f.repo.snapshot, task.id)->id == home.id);

   const auto snapshot = f.repo.snapshot;
   const auto count = f.repo.receipts.size();
   f.repo.failAfter = 1;
   rejects([&] { f.context(ContextOperation::DELETE_CONTEXT, home.id); });
   check(f.repo.snapshot == snapshot && f.repo.receipts.size() == count && f.repo.changes.size() == count);
   f.repo.failAfter = -1;
   const auto deletion = f.context(ContextOperation::DELETE_CONTEXT, home.id);
   check(!primaryContext(f.repo.snapshot, task.id) && deletion.patches.size() == 1);
   check(f.repo.snapshot.tasks == snapshot.tasks);
   f.commands.undo();
   check(primaryContext(f.repo.snapshot, task.id)->id == initialLink.id);
   f.commands.redo();
   check(!primaryContext(f.repo.snapshot, task.id));
   f.commands.undo();
   f.assign(task.id, office.id);
   check(primaryContext(f.repo.snapshot, task.id)->id != initialLink.id);
   f.commands.undo();
   check(primaryContext(f.repo.snapshot, task.id)->id == initialLink.id);
   f.commands.redo();
   check(effectiveContext(f.repo.snapshot, task.id)->id == office.id);
   check(std::get<Task>(*findEntity(f.repo.snapshot, entityKey(independent))) == independent);
}

void guardTests() {
   Fixture f;
   const auto moving = f.capture("Moving");
   const auto first = f.create("First");
   const auto last = f.create("Last");
   const auto other = f.capture("Other");
   f.assign(other.id, last.id);
   ContextCommand stale;
   stale.operation = ContextOperation::DELETE_CONTEXT;
   stale.id = first.id;
   stale.expectedRevision = first.localRevision;
   stale.expectedLinks.emplace();
   f.assign(moving.id, first.id);
   rejects([&] { f.commands.execute(stale); });

   // A new member in an earlier empty destination invalidates its undo witness.
   auto external = f.repo.snapshot;
   CommandService concurrent(f.repo, id(1), [&] { return ++f.now; }, [&] { return id(++f.nextId); });
   ContextCommand assign;
   assign.operation = ContextOperation::ASSIGN;
   assign.id = other.id;
   assign.expectedRevision = entityRevision(*findEntity(external, entityKey(other)));
   assign.destination = first.id;
   concurrent.execute(assign);
   const auto changed = f.repo.snapshot;
   rejects([&] { f.commands.undo(); });
   check(f.repo.snapshot == changed && f.commands.canUndo());

   // A revision-only external edit may not be masked by a successful local rename.
   Fixture ledger;
   auto root = ledger.create("Root");
   ++ledger.repo.snapshot.contexts.front().localRevision;
   const auto rename = ledger.context(ContextOperation::RENAME, root.id, "Renamed");
   ledger.commands.undo();
   check(!ledger.commands.canUndo() && rename.before.has_value());
}

void deleteCompensationConflictTests() {
   Fixture f;
   const auto first = f.capture("First");
   const auto second = f.capture("Second");
   const auto original = f.create("Original");
   const auto replacement = f.create("Replacement");
   f.assign(first.id, original.id);
   f.assign(second.id, original.id);
   const auto before = f.repo.snapshot;
   const auto receiptCount = f.repo.receipts.size();
   f.repo.failAfter = 2;
   rejects([&] { f.context(ContextOperation::DELETE_CONTEXT, original.id); });
   check(f.repo.snapshot == before && f.repo.receipts.size() == receiptCount && f.repo.changes.size() == receiptCount);
   f.repo.failAfter = -1;
   const auto deletion = f.context(ContextOperation::DELETE_CONTEXT, original.id);
   check(deletion.patches.size() == 2 && f.repo.snapshot.tasks == before.tasks);
   check(!primaryContext(f.repo.snapshot, first.id) && !primaryContext(f.repo.snapshot, second.id));

   CommandService concurrent(f.repo, id(1), [&] { return ++f.now; }, [&] { return id(++f.nextId); });
   ContextCommand assign;
   assign.operation = ContextOperation::ASSIGN;
   assign.id = second.id;
   assign.expectedRevision = second.localRevision + 1;
   assign.destination = replacement.id;
   concurrent.execute(assign);
   const auto changed = f.repo.snapshot;
   const auto beforeUndo = f.repo.receipts.size();
   rejects([&] { f.commands.undo(); });
   check(f.repo.snapshot == changed && f.repo.receipts.size() == beforeUndo && f.commands.canUndo());
   check(!primaryContext(f.repo.snapshot, first.id) && primaryContext(f.repo.snapshot, second.id)->contextId == replacement.id);
}

void normalizedUndoTests() {
   Fixture f;
   const auto context = f.create("Existing");
   const auto longRank = std::string(128, '1');
   f.repo.snapshot.contexts.front().sortKey = longRank;
   ++f.repo.snapshot.contexts.front().localRevision;
   const auto created = f.create("Inserted at exhausted tail");
   check(created.sortKey == "1");
   check(f.repo.snapshot.contexts.front().sortKey == "01");
   check(f.repo.receipts.back().patches.size() == 1);
   f.commands.undo();
   check(orderedContexts(f.repo.snapshot, true).size() == 1);
   check(orderedContexts(f.repo.snapshot, true).front().id == context.id);
   check(orderedContexts(f.repo.snapshot, true).front().sortKey == longRank);
   f.commands.redo();
   check(orderedContexts(f.repo.snapshot, true).back().id == created.id);
   check(f.repo.snapshot.contexts.front().sortKey == "01");

   Fixture independent;
   const auto task = independent.capture("Moving");
   const auto other = independent.capture("Separate");
   const auto target = independent.create("Destination");
   const auto outside = independent.create("Outside");
   independent.assign(other.id, outside.id);
   independent.assign(task.id, target.id);
   // An edit to a task in another partition does not invalidate placement.
   auto &separate = independent.repo.snapshot.tasks.back();
   ++separate.localRevision;
   separate.title = "Changed outside the guarded band";
   independent.commands.undo();
   check(!primaryContext(independent.repo.snapshot, task.id));
   check(primaryContext(independent.repo.snapshot, other.id)->contextId == outside.id);
}

void priorityTests() {
   constexpr auto highestNumber = std::numeric_limits<std::int64_t>::max();
   check(parseTaskPriority("1") == 1 && parseTaskPriority("9223372036854775807") == highestNumber);
   for (const auto *invalid : {"", "0", "-1", "+1", "1.5", "1e2", " 1", "1 ", "9223372036854775808", "１"}) {
      check(!parseTaskPriority(invalid));
   }
   check(!parseTaskPriority(std::string("1\0x", 3)));

   Fixture f;
   const auto task = f.capture("Change priority");
   const auto anchor = f.capture("Existing destination");
   const auto outside = f.capture("Outside the destination band");
   const auto revision = [&](const Task &record) {
      return entityRevision(*findEntity(f.repo.snapshot, entityKey(record)));
   };
   rejects([&] { f.commands.execute(TaskPriorityCommand{task.id, task.localRevision, 0}); });
   rejects([&] { f.commands.execute(TaskPriorityCommand{task.id, task.localRevision, 5}); });
   f.commands.execute(TaskPriorityCommand{anchor.id, revision(anchor), highestNumber});
   auto &storedAnchor = f.repo.snapshot.tasks.at(1);
   storedAnchor.manualOrderKey = std::string(MAX_RANK_BYTES, '1');
   ++storedAnchor.localRevision;
   const auto before = f.repo.snapshot;
   const TaskPriorityCommand edit{task.id, revision(task), highestNumber};
   f.repo.failAfter = 1;
   const auto receiptCount = f.repo.receipts.size();
   rejects([&] { f.commands.execute(edit); });
   check(f.repo.snapshot == before && f.repo.receipts.size() == receiptCount);
   f.repo.failAfter = -1;

   const auto change = f.commands.execute(edit);
   const auto &moved = std::get<Task>(change.after);
   check(moved.priority == highestNumber && moved.manualOrderKey == "1");
   check(change.changedFields == (PRIORITY | RANK) && change.patches.size() == 1);
   const auto &normalized = std::get<Task>(change.patches.front().after);
   check(normalized.id == anchor.id && normalized.priority == highestNumber && normalized.manualOrderKey == "01");
   check(f.repo.snapshot.tasks.at(2) == outside);
   check(f.commands.undoDescription() == "Set task priority");
   const auto encoded = QJsonDocument::fromJson(encodeReceiptSnapshot(change, false, 3).toUtf8()).object();
   check(decodeEntity(encoded["rootAfter"].toObject(), 3) == change.after);
   check(encoded["rootAfter"].toObject()["priority"].toString() == "9223372036854775807");

   f.commands.undo();
   check(f.repo.snapshot.tasks.at(0).priority == 5 && f.repo.snapshot.tasks.at(0).manualOrderKey == task.manualOrderKey);
   check(f.repo.snapshot.tasks.at(1).manualOrderKey == before.tasks.at(1).manualOrderKey);
   f.commands.redo();
   CommandService reopened(f.repo, id(1), [&] { return ++f.now; }, [&] { return id(++f.nextId); });
   check(reopened.read().tasks.at(0).priority == highestNumber && reopened.read().tasks.at(0).manualOrderKey == "1");
   check(!reopened.canUndo());
   ++f.repo.snapshot.tasks.at(1).localRevision;
   const auto changedExternally = f.repo.snapshot;
   rejects([&] { f.commands.undo(); });
   check(f.repo.snapshot == changedExternally);
   rejects([&] { f.commands.execute(edit); });
   f.repo.snapshot.schemaVersion = 2;
   rejects([&] { f.commands.execute(TaskPriorityCommand{task.id, revision(task), 1}); });
}

void priorityArchiveAndRedoTests() {
   Fixture f;
   const auto moving = f.capture("Archived assignment");
   const auto tail = f.capture("No context tail");
   const auto outside = f.capture("Other context");
   const auto joining = f.capture("Concurrent append");
   const auto archived = f.create("Archive");
   const auto active = f.create("Active");
   const auto row = [&](const Task &task) { return std::get<Task>(*findEntity(f.repo.snapshot, entityKey(task))); };
   f.assign(moving.id, archived.id);
   f.assign(outside.id, active.id);
   f.commands.execute(TaskPriorityCommand{outside.id, row(outside).localRevision, 1});
   f.commands.execute(TaskPriorityCommand{tail.id, row(tail).localRevision, 1});
   f.commands.execute(TaskPriorityCommand{joining.id, row(joining).localRevision, 7});
   f.context(ContextOperation::ARCHIVE, archived.id);

   const auto link = *primaryContext(f.repo.snapshot, moving.id);
   const auto otherContextTask = row(outside);
   const auto original = row(moving);
   check(!effectiveContext(f.repo.snapshot, moving.id));
   const auto changed = f.commands.execute(TaskPriorityCommand{moving.id, original.localRevision, 1});
   const auto &after = std::get<Task>(changed.after);
   check(after.manualOrderKey > row(tail).manualOrderKey);
   check(changed.patches.empty() && row(outside) == otherContextTask);
   check(*primaryContext(f.repo.snapshot, moving.id) == link);

   f.commands.undo();
   check(row(moving).priority == original.priority && row(moving).manualOrderKey == original.manualOrderKey);
   check(*primaryContext(f.repo.snapshot, moving.id) == link);
   CommandService concurrent(f.repo, id(1), [&] { return ++f.now; }, [&] { return id(++f.nextId); });
   concurrent.execute(TaskPriorityCommand{joining.id, row(joining).localRevision, 1});
   const auto beforeRedo = f.repo.snapshot;
   const auto count = f.repo.receipts.size();
   rejects([&] { f.commands.redo(); });
   check(f.repo.snapshot == beforeRedo && f.commands.canRedo());
   check(f.repo.receipts.size() == count && f.repo.changes.size() == count);
}

void priorityBoundedRefusalTests() {
   Fixture f;
   const auto moving = f.capture("Move into exhausted band");
   for (std::size_t i = 0; i <= MAX_RANK_WINDOW; ++i) {
      auto task = moving;
      task.id = id(1000 + static_cast<int>(i));
      task.priority = 1;
      task.manualOrderKey.assign(MAX_RANK_BYTES, '1');
      f.repo.snapshot.tasks.push_back(task);
   }
   const auto before = f.repo.snapshot;
   const auto count = f.repo.receipts.size();
   rejects([&] { f.commands.execute(TaskPriorityCommand{moving.id, moving.localRevision, 1}); });
   check(f.repo.snapshot == before && f.repo.receipts.size() == count && f.repo.changes.size() == count);
   check(f.commands.undoDescription() == "Create task");
}

void seededPriorityBandTests() {
   Fixture f;
   const auto one = f.capture("Zulu");
   const auto two = f.capture("Alpha");
   const auto three = f.capture("Middle");
   // Model the migration's UUID-ordered seed from a repository with different read order.
   auto &rows = f.repo.snapshot.tasks;
   std::ranges::reverse(rows);
   std::vector<std::string> ids{one.id, two.id, three.id};
   std::ranges::sort(ids);
   for (auto &row : rows) {
      const auto position = std::ranges::find(ids, row.id) - ids.begin() + 1;
      row.manualOrderKey = seedRank(position, ids.size());
   }
   PersistentTaskModel model;
   PersistentTaskFilter filter;
   filter.setSourceModel(&model);
   filter.sort(PersistentTaskModel::CONTEXT_COLUMN);
   auto assertOrder = [&]() {
      model.replaceWorkspace(f.repo.snapshot);
      for (int row = 0; row < filter.rowCount(); ++row) {
         check(filter.index(row, 0).data(Qt::UserRole).toString().toStdString() == ids.at(row));
      }
   };
   assertOrder();
   // Two merged bands can contain equal ranks. UUID remains the stable tie-breaker.
   for (auto &row : rows) { row.manualOrderKey = "1"; }
   assertOrder();
}

void taskMoveTests() {
   Fixture f;
   const auto moving = f.capture("Moving");
   const auto hidden = f.capture("Completed hidden");
   const auto early = f.capture("Early");
   const auto late = f.capture("Late");
   const auto from = f.create("Source");
   const auto to = f.create("Destination");
   const auto empty = f.create("Empty");
   const auto row = [&](const Task &task) { return std::get<Task>(*findEntity(f.repo.snapshot, entityKey(task))); };
   f.assign(moving.id, from.id);
   f.assign(hidden.id, to.id);
   f.assign(early.id, to.id);
   f.assign(late.id, to.id);
   f.commands.execute(TaskPriorityCommand{hidden.id, row(hidden).localRevision, 2});
   f.commands.execute(TaskPriorityCommand{early.id, row(early).localRevision, 2});
   f.commands.execute(TaskPriorityCommand{late.id, row(late).localRevision, 7});
   f.commands.execute(TaskCommand{TaskOperation::COMPLETE, hidden.id, row(hidden).localRevision, {}, {}});
   const auto sourceLink = *primaryContext(f.repo.snapshot, moving.id);
   const auto original = f.repo.snapshot;

   const auto before = previewTaskMove(original, {moving.id, to.id, TaskMovePosition::BEFORE, early.id});
   check(before.priority == 2 && before.bandPredecessor->key.id == hidden.id && before.visibleAnchor->key.id == early.id);
   check(previewTaskMove(original, {moving.id, to.id, TaskMovePosition::GROUP_END, {}}).priority == 7);
   const auto emptyMove = previewTaskMove(original, {moving.id, empty.id, TaskMovePosition::GROUP_END, {}});
   check(emptyMove.priority == 5 && !emptyMove.visibleAnchor);
   rejects([&] { previewTaskMove(original, {moving.id, to.id, TaskMovePosition::AFTER, early.id}); });
   check(original == f.repo.snapshot);
   const auto count = f.repo.receipts.size();
   f.repo.failAfter = 2;
   rejects([&] { f.commands.execute(before); });
   f.repo.failAfter = -1;
   check(f.repo.snapshot == original && f.repo.receipts.size() == count && f.repo.changes.size() == count);

   f.commands.execute(before);
   check(row(moving).priority == 2 && row(hidden).manualOrderKey < row(moving).manualOrderKey
         && row(moving).manualOrderKey < row(early).manualOrderKey);
   check(primaryContext(f.repo.snapshot, moving.id)->contextId == to.id && row(late).priority == 7);
   const auto movedReceipt = f.repo.receipts.back();
   const auto json = QJsonDocument::fromJson(encodeReceiptSnapshot(movedReceipt, false, 3).toUtf8()).object();
   check(json["movePreview"].toObject()["priority"].toString() == "2");
   check(json["movePreview"].toObject()["bandPredecessor"].toObject()["id"].toString().toStdString() == hidden.id);
   f.commands.undo();
   check(primaryContext(f.repo.snapshot, moving.id)->id == sourceLink.id && row(moving).priority == 5);
   f.commands.redo();
   check(row(moving).manualOrderKey == std::get<Task>(movedReceipt.after).manualOrderKey);
   f.commands.undo();
   rejects([&] { f.commands.execute(emptyMove); }); // Preview predates compensation revisions.
   f.commands.execute(previewTaskMove(f.repo.snapshot, {moving.id, empty.id, TaskMovePosition::GROUP_END, {}}));
   check(row(moving).priority == 5 && primaryContext(f.repo.snapshot, moving.id)->contextId == empty.id);

   f.context(ContextOperation::ARCHIVE, empty.id);
   const auto clear = previewTaskMove(f.repo.snapshot, {moving.id, {}, TaskMovePosition::GROUP_END, {}});
   check(!clear.noChange && clear.clearedContextId == empty.id && clear.sourceContextId.empty());
   f.commands.execute(clear);
   check(!primaryContext(f.repo.snapshot, moving.id));
   f.commands.undo();
   check(primaryContext(f.repo.snapshot, moving.id)->contextId == empty.id && !effectiveContext(f.repo.snapshot, moving.id));
}

void taskMoveNormalizationTests() {
   Fixture f;
   const auto first = f.capture("First");
   const auto second = f.capture("Second");
   const auto third = f.capture("Third");
   const auto equal = previewTaskMove(f.repo.snapshot, {second.id, {}, TaskMovePosition::BEFORE, third.id});
   const auto count = f.repo.receipts.size();
   check(equal.noChange);
   f.commands.execute(equal);
   check(f.repo.receipts.size() == count);
   const auto stale = previewTaskMove(f.repo.snapshot, {third.id, {}, TaskMovePosition::BEFORE, first.id});
   ++f.repo.snapshot.tasks.front().localRevision;
   rejects([&] { f.commands.execute(stale); });
   check(f.repo.receipts.size() == count);
   f.commands.execute(previewTaskMove(f.repo.snapshot, {third.id, {}, TaskMovePosition::BEFORE, first.id}));
   const auto &move = f.repo.receipts.back();
   check(move.changedFields == RANK && move.patches.empty());
   check(f.repo.snapshot.tasks.back().manualOrderKey < f.repo.snapshot.tasks.front().manualOrderKey);

   const auto target = f.create("Target");
   f.assign(second.id, target.id);
   auto &tail = f.repo.snapshot.tasks.at(1);
   tail.manualOrderKey.assign(MAX_RANK_BYTES, '1');
   ++tail.localRevision;
   const auto baseline = f.repo.snapshot;
   f.commands.execute(previewTaskMove(baseline, {third.id, target.id, TaskMovePosition::GROUP_END, {}}));
   check(f.repo.receipts.back().patches.size() == 2);
   check(f.repo.snapshot.tasks.at(1).manualOrderKey == "01" && f.repo.snapshot.tasks.at(2).manualOrderKey == "1");
   f.commands.undo();
   check(f.repo.snapshot.tasks.at(1).manualOrderKey == baseline.tasks.at(1).manualOrderKey);
   check(f.repo.snapshot.tasks.at(2).manualOrderKey == baseline.tasks.at(2).manualOrderKey);
   check(!primaryContext(f.repo.snapshot, third.id));
   f.commands.redo();
   check(primaryContext(f.repo.snapshot, third.id)->contextId == target.id);
}

void taskMoveMimeTests() {
   Fixture f;
   const auto moving = f.capture("Moving");
   const auto one = f.capture("One");
   const auto two = f.capture("Two");
   f.commands.execute(TaskPriorityCommand{one.id, one.localRevision, 1});
   f.commands.execute(TaskPriorityCommand{two.id, two.localRevision, 2});
   PersistentTaskModel model;
   PersistentTaskFilter proxy;
   proxy.setSourceModel(&model);
   model.replaceWorkspace(f.repo.snapshot);
   proxy.sort(PersistentTaskModel::CONTEXT_COLUMN);
   int submissions = 0;
   proxy.setTaskMoveCallbacks([&](const auto &target) -> std::optional<TaskMovePreview> {
      try { return previewTaskMove(f.repo.snapshot, target); }
      catch (const std::exception &) { return {}; }
   }, [&](const auto &preview) { ++submissions; f.commands.execute(preview); });
   check(!proxy.mimeData({proxy.index(2, 0)}));
   proxy.setTaskOrderingEnabled(true);
   std::unique_ptr<QMimeData> mime(proxy.mimeData({proxy.index(2, 0)}));
   check(mime && proxy.canDropMimeData(mime.get(), Qt::MoveAction, -1, 0, proxy.index(0, 0)));
   const auto between = proxy.previewTableDrop(mime.get(), proxy.index(0, 0), true);
   check(between && between->priority == 2 && between->visibleAnchor->key.id == two.id);
   check(!proxy.previewTableDrop(mime.get(), proxy.index(2, 0), true));
   QMimeData malformed;
   malformed.setData(proxy.mimeTypes().front(), "invalid");
   check(!proxy.dropMimeData(&malformed, Qt::MoveAction, 0, 0, {}));
   check(!proxy.dropMimeData(mime.get(), Qt::CopyAction, 0, 0, {}));
   check(submissions == 0);
   check(proxy.dropMimeData(mime.get(), Qt::MoveAction, 0, 0, {}));
   check(submissions == 1 && f.repo.snapshot.tasks.front().priority == 1);
   check(!proxy.dropMimeData(mime.get(), Qt::MoveAction, 0, 0, {})); // Stale drag MIME revision.
}

void modelAndCodecTests(const QStringList &arguments) {
   Fixture f;
   const auto unassigned = f.capture("A unassigned");
   const auto early = f.capture("Z early");
   const auto late = f.capture("B late");
   const auto home = f.create("Morning");
   const auto office = f.create("Office");
   f.assign(early.id, home.id);
   const auto receipt = f.assign(late.id, office.id);
   f.commands.execute(TaskPriorityCommand{early.id, early.localRevision + 1, 10});
   f.commands.execute(TaskPriorityCommand{unassigned.id, unassigned.localRevision, 2});

   PersistentTaskModel model;
   PersistentTaskFilter filter;
   filter.setSourceModel(&model);
   filter.setDynamicSortFilter(true);
   model.replaceWorkspace(f.repo.snapshot);
   filter.sort(PersistentTaskModel::CONTEXT_COLUMN);
   const auto actual = [&](int row) { return filter.index(row, 0).data(Qt::UserRole).toString().toStdString(); };
   check(actual(0) == early.id && actual(1) == late.id && actual(2) == unassigned.id);
   filter.sort(PersistentTaskModel::CONTEXT_COLUMN, Qt::DescendingOrder);
   check(actual(0) == early.id && actual(2) == unassigned.id);
   f.context(ContextOperation::ARCHIVE, home.id);
   model.replaceWorkspace(f.repo.snapshot);
   check(actual(0) == late.id);
   const auto archived = model.index(1, PersistentTaskModel::CONTEXT_COLUMN);
   check(archived.data().toString() == "No context" && archived.data(Qt::ToolTipRole).toString().contains("Morning"));
   filter.setIncludeArchived(true);
   check(actual(0) == early.id && archived.data().toString().contains("Morning"));
   filter.sort(PersistentTaskModel::PRIORITY_COLUMN, Qt::AscendingOrder);
   check(actual(0) == unassigned.id && actual(1) == late.id && actual(2) == early.id);
   filter.sort(PersistentTaskModel::TITLE_COLUMN, Qt::AscendingOrder);
   check(actual(0) == unassigned.id);

   const auto document = QJsonDocument::fromJson(encodeReceiptSnapshot(receipt, false, 3).toUtf8()).object();
   check(document["assignment"].toObject()["destinationContextId"].toString().toStdString() == office.id);
   check(document["version"].toInt() == 3 && document["patches"].toArray().size() == 1);
   check(decodeEntity(document["rootAfter"].toObject(), 3) == receipt.after);
   const auto patch = document["patches"].toArray().first().toObject();
   check(decodeEntity(patch["after"].toObject(), 3) == receipt.patches.front().after);
   check(document["rootAfter"].toObject()["priority"].toString() == "5");
   check(!QJsonDocument::fromJson(encodeReceiptSnapshot(receipt, true, 3).toUtf8()).object().contains("rootAfter"));
   check(!QJsonDocument::fromJson(encodeReceiptSnapshot(receipt, false).toUtf8()).object().contains("priority"));
   const DatabaseConfig fixtureRuntime{"127.0.0.1", 3306, "taskcompass_phase13_test", "taskcompass_phase13_app", "offline-fixture"};
   fixtureRuntime.validate();
   auto wrong = fixtureRuntime;
   wrong.databaseName = "taskcompass_dev";
   rejects([&] { wrong.validate(); });
   check(MigrationRunner::migrationStatements(3).size() == 8 && MigrationRunner::tableNamesForVersion(3).size() == 13);
   check(arguments.size() == 3);
   QFile resource(arguments[1]);
   check(resource.open(QIODevice::ReadOnly) && resource.readAll() == MigrationRunner::migrationSql(3));
   QProcess preview;
   preview.start(arguments[2], {"--print-migration", "3"});
   check(preview.waitForFinished(5000) && preview.exitCode() == 0
         && preview.readAllStandardOutput().replace("\r\n", "\n") == MigrationRunner::migrationSql(3));
   // Every candidate below must fail before opening a connection.
   auto environment = QProcessEnvironment::systemEnvironment();
   for (const auto &key : environment.keys()) {
      if (key.startsWith("TASKCOMPASS_")) { environment.remove(key); }
   }
   const QStringList expected{"--expected-dataset", QString::fromStdString(id(1)),
                              "--expected-baseline-fingerprint", QString(64, 'a')};
   const auto rejectsCli = [&](const QStringList &flags, const QByteArray &reason) {
      QProcess blocked;
      blocked.setProcessEnvironment(environment);
      blocked.start(arguments[2], flags);
      check(blocked.waitForFinished(5000) && blocked.exitStatus() == QProcess::NormalExit
            && blocked.exitCode() == 1 && blocked.readAllStandardError().contains(reason));
   };
   const QStringList apply{"--apply", "--to-version", "3"};
   rejectsCli(apply, "003 requires --phase13-test");
   rejectsCli(apply + QStringList{"--phase12-test"} + expected, "003 requires --phase13-test");
   rejectsCli(apply + QStringList{"--integration"} + expected, "003 requires --phase13-test");
   rejectsCli(apply + QStringList{"--phase13-test"}, "003 requires --phase13-test");
   environment.insert("TASKCOMPASS_PHASE13_TEST_DB_ISOLATED", "1");
   environment.insert("TASKCOMPASS_PHASE13_TEST_DB_HOST", "127.0.0.1");
   environment.insert("TASKCOMPASS_PHASE13_TEST_DB_PORT", "3306");
   environment.insert("TASKCOMPASS_PHASE13_TEST_DB_USER", "taskcompass_phase13_test");
   environment.insert("TASKCOMPASS_PHASE13_TEST_DB_PASSWORD", "offline-fixture");
   for (const auto *target : {"taskcompass_dev", "taskcompass_integration", "taskcompass_phase12_test"}) {
      environment.insert("TASKCOMPASS_PHASE13_TEST_DB_NAME", target);
      rejectsCli(apply + QStringList{"--phase13-test"} + expected, "Explicit isolated Phase 1.3");
   }

}

}

int main(int argc, char **argv) {
   QApplication application(argc, argv);
   try {
      rankTests();
      contextTests();
      guardTests();
      deleteCompensationConflictTests();
      normalizedUndoTests();
      priorityTests();
      priorityArchiveAndRedoTests();
      priorityBoundedRefusalTests();
      seededPriorityBandTests();
      taskMoveTests();
      taskMoveNormalizationTests();
      taskMoveMimeTests();
      modelAndCodecTests(application.arguments());
      std::cout << "PASS: fractional ranks, context transactions, inverse receipts, schema-3 preview and grouping.\n";
      return 0;
   } catch (const std::exception &error) {
      std::cerr << error.what() << '\n';
      return 1;
   }
}
