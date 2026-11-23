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
#include <QProcess>
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

void modelAndCodecTests(const QStringList &arguments) {
   Fixture f;
   const auto unassigned = f.capture("A unassigned");
   const auto early = f.capture("Z early");
   const auto late = f.capture("B late");
   const auto home = f.create("Morning");
   const auto office = f.create("Office");
   f.assign(early.id, home.id);
   const auto receipt = f.assign(late.id, office.id);

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
   // Missing explicit fixture arguments must fail before reading inherited target settings.
   QProcess blocked;
   blocked.start(arguments[2], {"--apply", "--to-version", "3"});
   check(blocked.waitForFinished(5000) && blocked.exitCode() == 1
         && blocked.readAllStandardError().contains("003 requires --phase13-test"));
}

}

int main(int argc, char **argv) {
   QApplication application(argc, argv);
   try {
      rankTests();
      contextTests();
      guardTests();
      normalizedUndoTests();
      modelAndCodecTests(application.arguments());
      std::cout << "PASS: fractional ranks, context transactions, inverse receipts, schema-3 preview and grouping.\n";
      return 0;
   } catch (const std::exception &error) {
      std::cerr << error.what() << '\n';
      return 1;
   }
}
