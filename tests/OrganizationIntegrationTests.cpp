#include "OrganizationTestSafety.h"
#include "application/CommandService.h"
#include "application/TaskService.h"
#include "infrastructure/storage/DatabaseConnection.h"
#include "infrastructure/storage/MigrationRunner.h"
#include "infrastructure/storage/MariaDbTaskRepository.h"
#include "infrastructure/storage/MariaDbWorkspaceRepository.h"
#include "infrastructure/storage/MutationLease.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTextStream>
#include <QUuid>
#include <atomic>
#include <barrier>
#include <future>
#include <algorithm>

using namespace taskcompass;
namespace {
void check(bool value, const char *message) { if (!value) { throw std::runtime_error(message); } }
template<typename Action> void rejects(Action action) {
   bool failed = false;
   try { action(); } catch (const std::exception &) { failed = true; }
   check(failed, "Expected guarded operation to reject");
}
std::string uuid() { return QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString(); }
Timestamp now() { return QDateTime::currentMSecsSinceEpoch(); }
void identity(QSqlDatabase database) {
   check(database.isOpen() && database.driverName() == "QMYSQL" && database.hostName() == "127.0.0.1"
         && database.port() == 3306
         && database.databaseName() == "taskcompass_phase12_test" && database.userName() == "taskcompass_phase12_test", "Wrong Phase 1.2 test identity; refusing SQL.");
   QSqlQuery query(database);
   check(query.exec("SELECT DATABASE(),CURRENT_USER(),@@foreign_key_checks,@@check_constraint_checks") && query.next()
         && query.value(0).toString() == "taskcompass_phase12_test" && query.value(1).toString() == "taskcompass_phase12_test@127.0.0.1"
         && query.value(2).toInt() == 1 && query.value(3).toInt() == 1, "Authenticated identity/enforcement differs; stop.");
}
IntegrationTestSafety::Catalog catalog(QSqlDatabase database) {
   identity(database);
   IntegrationTestSafety::Catalog result;
   QSqlQuery query(database);
   check(query.exec("SELECT TABLE_NAME,TABLE_TYPE,ENGINE FROM information_schema.TABLES WHERE TABLE_SCHEMA='taskcompass_phase12_test'"), "Cannot inspect Phase 1.2 tables.");
   while (query.next()) { result.tables.append({query.value(0).toString(), query.value(1).toString(), query.value(2).toString()}); }
   auto names = [&](const QString &sql) {
      QStringList list; check(query.exec(sql), "Cannot inspect stored code; stop.");
      while (query.next()) { list.append(query.value(0).toString()); }
      return list;
   };
   result.routines = names("SELECT ROUTINE_NAME FROM information_schema.ROUTINES WHERE ROUTINE_SCHEMA='taskcompass_phase12_test'");
   result.triggers = names("SELECT TRIGGER_NAME FROM information_schema.TRIGGERS WHERE TRIGGER_SCHEMA='taskcompass_phase12_test'");
   result.events = names("SELECT EVENT_NAME FROM information_schema.EVENTS WHERE EVENT_SCHEMA='taskcompass_phase12_test'");
   return result;
}
void cleanupOwned(QSqlDatabase database, const OrganizationTestSafety &safety) {
   safety.requireCleanup(catalog(database));
   for (const auto &table : OrganizationTestSafety::tables()) {
      QSqlQuery query(database);
      check(query.exec("DROP TABLE IF EXISTS `taskcompass_phase12_test`.`" + table + "`"), "Run-owned cleanup failed; preserve remaining state.");
   }
   check(catalog(database).tables.isEmpty(), "Test cleanup did not leave an empty schema.");
}
QByteArray legacyValues(QSqlDatabase database) {
   QJsonArray tables;
   const QList<QString> sqls = {
      "SELECT id,name,createdAt FROM Dataset ORDER BY id",
      "SELECT id,datasetId,title,notes,lifecycleStatus,createdAt,updatedAt,completedAt,deletedAt,localRevision FROM Task ORDER BY id",
      "SELECT id,operationId,datasetId,taskId,commandType,payloadVersion,recordedAt,beforeSnapshot,afterSnapshot,resultingRevision,compensatesCommandId,reappliesCommandId FROM CommandRecord ORDER BY id",
      "SELECT id,operationId,datasetId,taskId,localRevision,payloadVersion,immutablePayload,createdAt,acknowledgmentState FROM LocalChange ORDER BY id",
      "SELECT version,checksum,schemaFingerprint,appliedAt FROM SchemaMigration WHERE version=1"};
   for (const auto &sql : sqls) {
      QSqlQuery query(database); check(query.exec(sql), "Cannot snapshot fixture legacy values.");
      QJsonArray rows;
      while (query.next()) {
         QJsonArray row;
         for (int index = 0; index < query.record().count(); ++index) {
            row.append(query.value(index).isNull() ? QJsonValue(QJsonValue::Null) : QJsonValue(query.value(index).toString()));
         }
         rows.append(row);
      }
      tables.append(rows);
   }
   return QJsonDocument(tables).toJson(QJsonDocument::Compact);
}
void rejectsConstraint(QSqlDatabase database, const QString &sql, const QVariantList &values, const char *constraint) {
   check(database.transaction(), "Cannot start negative constraint fixture.");
   QSqlQuery query(database);
   check(query.prepare(sql), "Cannot prepare negative constraint fixture.");
   for (const auto &value : values) { query.addBindValue(value); }
   const bool accepted = query.exec();
   const bool expected = query.lastError().text().contains(QString::fromLatin1(constraint), Qt::CaseInsensitive);
   check(database.rollback(), "Cannot roll back negative constraint fixture; preserve state.");
   check(!accepted && expected, "Expected SQL constraint did not reject fixture for the intended reason.");
}
void rejectsHistoryTampering(QSqlDatabase database) {
   for (const auto &sql : {
      "UPDATE SchemaMigration SET version=3 WHERE version=2",
      "UPDATE SchemaMigration SET checksum=REPEAT('0',64) WHERE version=2",
      "UPDATE SchemaMigration SET schemaFingerprint=REPEAT('0',64) WHERE version=2",
      "UPDATE MigrationAttempt SET attemptState='failed',completedAt=NULL WHERE version=2",
      "UPDATE MigrationAttempt SET baselineFingerprint=REPEAT('0',64) WHERE version=2"}) {
      check(database.transaction(), "Cannot start history validation fixture.");
      QSqlQuery query(database); check(query.exec(sql) && query.numRowsAffected() == 1, "Cannot inject history validation fixture.");
      rejects([&]() { MigrationRunner(database).verify(2); });
      check(database.rollback(), "History validation fixture rollback failed.");
      MigrationRunner(database).verify(2);
   }
}
OrganizationCommand create(OrganizationKind kind, const char *title, std::optional<OrganizationKey> parent = {}) {
   OrganizationCommand command; command.kind = kind; command.title = title; command.parent = parent; return command;
}
}

int main(int argc, char **argv) {
   QCoreApplication application(argc, argv);
   if (!OrganizationTestSafety::hasOptIn(application.arguments())) {
      QTextStream(stdout) << "SKIPPED: Phase 1.2 live tests require --run --allow-reset and an approved empty taskcompass_phase12_test only.\n";
      return 77;
   }
   try {
      const auto config = DatabaseConfig::fromPhase12Environment();
      DatabaseConnection connection(config); auto database = connection.database(); identity(database);
      QSqlQuery lock(database);
      check(lock.exec("SELECT GET_LOCK('TaskCompassMigration:taskcompass_phase12_test',0)") && lock.next() && lock.value(0).toInt() == 1, "Another migration/test owns the target.");
      struct Release {
         QSqlDatabase database;
         ~Release() { QSqlQuery query(database); query.exec("SELECT RELEASE_LOCK('TaskCompassMigration:taskcompass_phase12_test')"); }
      } release{database};
      check(lock.exec("SELECT @@lower_case_table_names") && lock.next(), "Cannot inspect case mode.");
      OrganizationTestSafety safety; safety.begin(catalog(database), lock.value(0).toInt());
      // Every fixture/reset below is after this process's completely empty proof.
      // Intentional failure cases have explicit run-owned cleanup; unexpected
      // exceptions leave evidence (no catch/finally reset).
      for (int failedStep = 0; failedStep <= 11; ++failedStep) {
         MigrationRunner(database).applyInitial();
         bool injected = false;
         rejects([&]() {
            MigrationRunner interrupted(database, [failedStep, &injected](int step) { if (step == failedStep) { injected = true; throw std::runtime_error("Injected migration interruption"); } });
            interrupted.applyToVersion(2);
         });
         check(injected, "Unexpected migration failure before injection; preserve state for explicit review.");
         rejects([&]() { MigrationRunner(database).verify(2); });
         rejects([&]() { MigrationRunner(database).applyToVersion(2); }); // Never resume/repair.
         cleanupOwned(database, safety);
      }
      MigrationRunner runner(database); runner.applyInitial();
      const auto dataset = runner.verify(1);
      MariaDbTaskRepository legacy(database, dataset);
      TaskService old(legacy, dataset, now, uuid);
      const auto task = old.execute({TaskOperation::CREATE, {}, 0, "Existing 🧭 task", ""});
      old.execute({TaskOperation::COMPLETE, task.after.id, 1, {}, {}});
      old.execute({TaskOperation::CREATE, {}, 0, "Tombstone", {}}); old.undo();
      const auto before = legacyValues(database);
      runner.applyToVersion(2);
      check(runner.verify(2) == dataset && legacyValues(database) == before, "Upgrade rewrote schema-1 data/history/dataset.");
      runner.applyToVersion(2); // Verified idempotent, no duplicate attempts.
      rejects([&]() { runner.applyInitial(); });
      MariaDbWorkspaceRepository repository(database, dataset);
      CommandService service(repository, dataset, now, uuid);
      auto pillar = service.execute(create(OrganizationKind::PILLAR, "Responsibility"));
      const OrganizationKey pillarKey{OrganizationKind::PILLAR, entityKey(pillar.after).id};
      auto project = service.execute(create(OrganizationKind::PROJECT, "Project", pillarKey));
      const OrganizationKey projectKey{OrganizationKind::PROJECT, entityKey(project.after).id};
      auto objective = service.execute(create(OrganizationKind::OBJECTIVE, "Outcome", projectKey));
      TaskCommand capture{TaskOperation::CREATE, {}, 0, "Owned task", {}};
      capture.ownerChange = {TaskOwnerKind::OBJECTIVE, entityKey(objective.after).id};
      auto owned = service.execute(capture);
      // Constraint probes exist only inside this fresh, run-owned synthetic schema.
      const auto pillarId = QString::fromStdString(pillarKey.id), projectId = QString::fromStdString(projectKey.id);
      const auto objectiveId = QString::fromStdString(entityKey(objective.after).id), taskId = QString::fromStdString(entityKey(owned.after).id);
      rejectsConstraint(database, "UPDATE Pillar SET parentPillarId=? WHERE id=?", {pillarId, pillarId}, "pillarNotSelf");
      rejectsConstraint(database, "UPDATE Objective SET pillarId=NULL,projectId=NULL WHERE id=?", {objectiveId}, "objectiveParentChoice");
      rejectsConstraint(database, "UPDATE Project SET pillarId=NULL,parentProjectId=NULL WHERE id=?", {projectId}, "projectParentChoice");
      rejectsConstraint(database, "UPDATE Project SET parentProjectId=?,pillarId=NULL WHERE id=?", {projectId, projectId}, "projectNotSelf");
      rejectsConstraint(database, "UPDATE Objective SET pillarId=? WHERE id=?", {pillarId, objectiveId}, "objectiveParentChoice");
      rejectsConstraint(database, "UPDATE Task SET directProjectId=? WHERE id=?", {projectId, taskId}, "taskOwnerChoice");
      rejectsConstraint(database, "UPDATE Task SET objectiveId=? WHERE id=?", {QString::fromStdString(uuid()), taskId}, "taskObjectiveScope");
      rejectsConstraint(database, "UPDATE CommandRecord SET taskId=? WHERE id=?", {taskId, QString::fromStdString(pillar.id)}, "commandTargetV2");
      rejectsConstraint(database, "UPDATE LocalChange SET taskId=? WHERE id=?", {taskId, QString::fromStdString(pillar.id)}, "changeTargetV2");
      rejectsConstraint(database, "UPDATE CommandRecord SET commandType='complete' WHERE id=?", {QString::fromStdString(pillar.id)}, "commandLifecycleTargetV2");
      // Same UUID in a different dataset must not satisfy any composite owner FK.
      check(database.transaction(), "Cannot start cross-dataset fixture.");
      const auto foreignDataset = QString::fromStdString(uuid());
      QSqlQuery foreign(database);
      foreign.prepare("INSERT INTO Dataset (id,name,createdAt) VALUES (?,'Negative fixture',1000)"); foreign.addBindValue(foreignDataset);
      check(foreign.exec(), "Cannot create run-owned foreign-dataset fixture.");
      const auto foreignPillar = QString::fromStdString(uuid()), foreignProject = QString::fromStdString(uuid()), foreignObjective = QString::fromStdString(uuid());
      auto insertForeign = [&](const QString &sql, const QVariantList &values) {
         check(foreign.prepare(sql), "Cannot prepare cross-dataset fixture.");
         for (const auto &value : values) { foreign.addBindValue(value); }
         check(foreign.exec(), "Cannot create cross-dataset fixture.");
      };
      insertForeign("INSERT INTO Pillar (id,datasetId,name,createdAt,updatedAt,localRevision) VALUES (?,?,'Foreign pillar',1000,1000,1)", {foreignPillar, foreignDataset});
      insertForeign("INSERT INTO Project (id,datasetId,pillarId,title,createdAt,updatedAt,localRevision) VALUES (?,?,?,'Foreign project',1000,1000,1)", {foreignProject, foreignDataset, foreignPillar});
      insertForeign("INSERT INTO Objective (id,datasetId,pillarId,title,createdAt,updatedAt,localRevision) VALUES (?,?,?,'Foreign objective',1000,1000,1)", {foreignObjective, foreignDataset, foreignPillar});
      const QList<QPair<QString, QVariantList>> invalidScopes = {
         {"UPDATE Pillar SET parentPillarId=? WHERE id=?", {foreignPillar, pillarId}},
         {"UPDATE Project SET pillarId=? WHERE id=?", {foreignPillar, projectId}},
         {"UPDATE Objective SET projectId=NULL,pillarId=? WHERE id=?", {foreignPillar, objectiveId}},
         {"UPDATE Task SET objectiveId=? WHERE id=?", {foreignObjective, taskId}},
         {"UPDATE Task SET objectiveId=NULL,directProjectId=? WHERE id=?", {foreignProject, taskId}}};
      for (const auto &[sql, values] : invalidScopes) {
         check(foreign.prepare(sql), "Cannot prepare negative cross-dataset fixture.");
         for (const auto &value : values) { foreign.addBindValue(value); }
         check(!foreign.exec() && foreign.lastError().nativeErrorCode() == "1452", "Cross-dataset composite FK failed to reject ownership.");
      }
      check(database.rollback(), "Cross-dataset fixture rollback failed.");
      rejectsHistoryTampering(database);
      auto complete = service.execute({TaskOperation::COMPLETE, entityKey(owned.after).id, 1, {}, {}});
      const auto completion = std::get<Task>(complete.after).completedAt;
      service.undo(); service.redo();
      const auto state = service.read();
      const auto completedTask = std::find_if(state.tasks.begin(), state.tasks.end(), [&](const auto &value) { return value.id == entityKey(owned.after).id; });
      check(completedTask != state.tasks.end() && completedTask->completedAt == completion, "Completion inverse failed");
      OrganizationCommand archive;
      archive.operation = OrganizationOperation::ARCHIVE; archive.kind = OrganizationKind::PILLAR; archive.id = pillarKey.id; archive.expectedRevision = 1;
      service.execute(archive);
      check(OrganizationRules(dataset, service.read().organizations).taskArchived(std::get<Task>(owned.after)), "Archive not inherited.");
      service.undo();
      const auto stable = legacyValues(database);
      const auto stableWorkspace = repository.read();
      for (int failedWrite = 0; failedWrite < 3; ++failedWrite) {
         bool injected = false;
         MariaDbWorkspaceRepository failing(database, dataset, [failedWrite, &injected](int step) { if (step == failedWrite) { injected = true; throw std::runtime_error("Injected transaction failure"); } });
         CommandService commands(failing, dataset, now, uuid);
         rejects([&]() { commands.execute(TaskCommand{TaskOperation::CREATE, {}, 0, "Rollback", {}}); });
         check(injected, "Task rollback injection was not reached; preserve unexpected failure.");
         injected = false;
         rejects([&]() { commands.execute(create(OrganizationKind::PROJECT, "Rollback organization", pillarKey)); });
         check(injected, "Organization rollback injection was not reached; preserve unexpected failure.");
         check(legacyValues(database) == stable && repository.read() == stableWorkspace && !commands.canUndo(), "Failed transaction leaked entity/history/outbox.");
      }
      // Separate connections must observe busy leases and release on connection loss.
      {
         MutationLease lease(database);
         auto blocked = std::async(std::launch::async, [config]() {
            DatabaseConnection second(config);
            try { MutationLease locked(second.database(), 0); return false; }
            catch (const std::exception &error) {
               QSqlQuery alive(second.database());
               return QString::fromUtf8(error.what()).contains("lease not acquired")
                      && alive.exec("SELECT 1") && alive.next() && alive.value(0).toInt() == 1;
            }
         });
         check(blocked.get(), "Second connection bypassed busy mutation lease.");
      }
      {
         DatabaseConnection lost(config);
         MutationLease lease(lost.database()); lost.database().close();
         DatabaseConnection survivor(config); MutationLease available(survivor.database(), 2);
      }
      // Opposing moves race from separate connections; exactly one can commit.
      auto first = service.execute(create(OrganizationKind::PILLAR, "A"));
      auto second = service.execute(create(OrganizationKind::PILLAR, "B"));
      const auto firstId = entityKey(first.after).id, secondId = entityKey(second.after).id;
      std::barrier ready(2);
      auto move = [&](std::string id, std::string parent) {
         ready.arrive_and_wait();
         try {
            DatabaseConnection other(config); MariaDbWorkspaceRepository storage(other.database(), dataset);
            CommandService commands(storage, dataset, now, uuid);
            OrganizationCommand request; request.operation = OrganizationOperation::MOVE; request.kind = OrganizationKind::PILLAR;
            request.id = id; request.expectedRevision = 1; request.parent = OrganizationKey{OrganizationKind::PILLAR, parent};
            commands.execute(request); return 1;
         } catch (const std::exception &error) {
            return QString::fromUtf8(error.what()).contains("cycle", Qt::CaseInsensitive) ? 0 : -1;
         }
      };
      auto a = std::async(std::launch::async, move, firstId, secondId);
      auto b = std::async(std::launch::async, move, secondId, firstId);
      const int firstResult = a.get(), secondResult = b.get();
      check((firstResult == 1 && secondResult == 0) || (firstResult == 0 && secondResult == 1), "Conflicting moves did not serialize/reject the cycle.");
      OrganizationRules(dataset, repository.read().organizations);
      // Reopen with a genuinely new connection; state persists, session stack does not.
      {
         DatabaseConnection reopened(config); MariaDbWorkspaceRepository storage(reopened.database(), dataset);
         CommandService commands(storage, dataset, now, uuid);
         check(!commands.canUndo() && commands.read().organizations.size() == repository.read().organizations.size(), "Restart persistence/history policy failed.");
      }
      cleanupOwned(database, safety);
      QTextStream(stdout) << "PASS: schema-1 preservation/upgrade, partial-migration refusal, typed commands/archive/history, atomic rollback, separate-connection lease timeout/loss/conflicting moves and restart. Phase 1.2 test schema left empty.\n";
      return 0;
   } catch (const std::exception &error) {
      QTextStream(stderr) << error.what() << "\nSTOP: preserve Phase 1.2 test state; no automatic recovery/reset.\n";
      return 1;
   }
}
