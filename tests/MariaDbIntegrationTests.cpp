#include "IntegrationTestSafety.h"
#include "application/TaskService.h"
#include "infrastructure/storage/DatabaseConnection.h"
#include "infrastructure/storage/MariaDbTaskRepository.h"
#include "infrastructure/storage/MigrationRunner.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QSqlQuery>
#include <QTextStream>
#include <QUuid>
#include <QVariant>
#include <stdexcept>

using namespace taskcompass;

namespace {
void check(bool condition, const char *message) {
   if (!condition) {
      throw std::runtime_error(message);
   }
}
void execute(QSqlDatabase database, const QString &sql) {
   QSqlQuery query(database);
   check(query.exec(sql), "Integration SQL failed; inspect isolated test database and approved grants.");
}
template<typename Operation> void rejects(Operation operation) {
   bool rejected = false;
   try { operation(); } catch (const std::exception &) { rejected = true; }
   check(rejected, "Expected rejection did not occur");
}
std::string uuid() { return QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString(); }
int count(QSqlDatabase database, const QString &table) {
   // All table arguments below are literal test-owned identifiers.
   QSqlQuery query(database);
   check(query.exec("SELECT COUNT(*) FROM " + table) && query.next(), "Cannot count test rows.");
   return query.value(0).toInt();
}
void requireTestIdentity(QSqlDatabase database) {
   check(database.isOpen() && database.driverName() == "QMYSQL" && database.hostName() == "127.0.0.1"
         && database.databaseName() == "taskcompass_integration" && database.userName() == "taskcompass_test",
         "Integration connection metadata guard failed.");
   QSqlQuery identity(database);
   check(identity.exec("SELECT DATABASE(), CURRENT_USER(), @@foreign_key_checks, @@check_constraint_checks") && identity.next()
         && identity.value(0).toString() == "taskcompass_integration"
         && identity.value(1).toString() == "taskcompass_test@127.0.0.1"
         && identity.value(2).toInt() == 1 && identity.value(3).toInt() == 1,
         "Integration authenticated identity/enforcement guard failed.");
}

IntegrationTestSafety::Catalog readCatalog(QSqlDatabase database) {
   requireTestIdentity(database);
   IntegrationTestSafety::Catalog catalog;
   QSqlQuery tables(database);
   check(tables.exec("SELECT TABLE_NAME,TABLE_TYPE,ENGINE FROM information_schema.TABLES WHERE TABLE_SCHEMA='taskcompass_integration'"),
         "Cannot inspect integration tables; no reset permitted.");
   while (tables.next()) {
      catalog.tables.append({tables.value(0).toString(), tables.value(1).toString(), tables.value(2).toString()});
   }
   auto readNames = [&database](const QString &sql) {
      QSqlQuery query(database);
      check(query.exec(sql), "Cannot inspect integration stored objects; no reset permitted.");
      QStringList names;
      while (query.next()) {
         names.append(query.value(0).toString());
      }
      return names;
   };
   catalog.routines = readNames("SELECT ROUTINE_NAME FROM information_schema.ROUTINES WHERE ROUTINE_SCHEMA='taskcompass_integration'");
   catalog.triggers = readNames("SELECT TRIGGER_NAME FROM information_schema.TRIGGERS WHERE TRIGGER_SCHEMA='taskcompass_integration'");
   catalog.events = readNames("SELECT EVENT_NAME FROM information_schema.EVENTS WHERE EVENT_SCHEMA='taskcompass_integration'");
   return catalog;
}

void resetTestSchema(QSqlDatabase database, const IntegrationTestSafety &safety) {
   safety.requireResetAllowed(readCatalog(database));
   const QStringList tables = {"LocalChange", "CommandRecord", "Task", "Dataset", "SchemaMigration"};
   for (const auto &table : tables) {
      execute(database, "DROP TABLE IF EXISTS `taskcompass_integration`.`" + table + "`");
   }
   const auto remaining = readCatalog(database);
   safety.requireResetAllowed(remaining);
   check(remaining.tables.isEmpty(), "Integration reset did not leave an empty schema; stop and inspect.");
}

}

int main(int argc, char *argv[]) {
   QCoreApplication application(argc, argv);
   if (!IntegrationTestSafety::hasExplicitOptIn(application.arguments())) {
      QTextStream(stdout) << "SKIPPED: live tests require explicit --run --allow-reset and isolated TASKCOMPASS_TEST_DB_* configuration.\n";
      return 77;
   }
   try {
      const auto config = DatabaseConfig::fromEnvironment(true);
      check(config.user == "taskcompass_test", "Integration tests require the dedicated taskcompass_test account.");
      DatabaseConnection connection(config);
      auto database = connection.database();
      requireTestIdentity(database);
      QSqlQuery lease(database);
      check(lease.exec("SELECT GET_LOCK('TaskCompassMigration:taskcompass_integration',0)")
            && lease.next() && lease.value(0).toInt() == 1,
            "Another integration/migration process owns this schema; refusing to start.");
      struct LeaseRelease {
         QSqlDatabase database;
         ~LeaseRelease() {
            QSqlQuery query(database);
            query.exec("SELECT RELEASE_LOCK('TaskCompassMigration:taskcompass_integration')");
         }
      } release{database};
      QSqlQuery caseSetting(database);
      check(caseSetting.exec("SELECT @@lower_case_table_names") && caseSetting.next(), "Cannot inspect table-name case mode.");
      IntegrationTestSafety safety;
      safety.begin(readCatalog(database), caseSetting.value(0).toInt()); // Never delete existing objects at startup.
      MigrationRunner migrations(database);
      rejects([&]() { migrations.verify(); });
      // Simulate a DDL failure after the first implicitly committed statement.
      execute(database, QString::fromUtf8(MigrationRunner::migrationSql()).split(';').first());
      rejects([&]() { migrations.applyInitial(); });
      resetTestSchema(database, safety);
      migrations.applyInitial();
      const auto datasetId = migrations.verify();
      migrations.applyInitial();
      check(migrations.verify() == datasetId, "Migration rerun changed dataset");
      execute(database, "UPDATE SchemaMigration SET version=99");
      rejects([&]() { migrations.verify(); });
      execute(database, "UPDATE SchemaMigration SET version=1");
      execute(database, "UPDATE SchemaMigration SET checksum=REPEAT('0',64)");
      rejects([&]() { migrations.verify(); });
      resetTestSchema(database, safety);
      migrations.applyInitial();
      const auto dataset = migrations.verify();
      MariaDbTaskRepository repository(database, dataset);
      Timestamp now = QDateTime::currentMSecsSinceEpoch();
      TaskService service(repository, dataset, [&now]() { return now; }, uuid);
      auto created = service.execute({TaskOperation::CREATE, {}, 0, "Persistent 🧭 ' ? \\ task", "notes"});
      const auto id = created.after.id;
      check(repository.find(id) == std::optional(created.after), "Task did not round-trip");
      auto edit = service.execute({TaskOperation::EDIT, id, 1, "Edited", {}});
      auto completed = service.execute({TaskOperation::COMPLETE, id, 2, {}, {}});
      check(completed.after.completedAt.has_value(), "Completion timestamp missing");
      auto reopened = service.execute({TaskOperation::REOPEN, id, 3, {}, {}});
      check(!reopened.after.completedAt, "Reopen failed");
      for (int index = 0; index < 4; ++index) {
         service.undo();
      }
      check(repository.list().empty(), "Undo creation did not hide tombstone");
      for (int index = 0; index < 4; ++index) {
         service.redo();
      }
      check(repository.find(id)->title == edit.after.title, "Redo failed");
      const auto beforeFailure = repository.find(id);
      const auto receiptCount = count(database, "CommandRecord");
      const auto changeCount = count(database, "LocalChange");
      MariaDbTaskRepository faulty(database, dataset, []() { throw std::runtime_error("Injected post-task-write failure"); });
      TaskService failing(faulty, dataset, [&now]() { return now; }, uuid);
      rejects([&]() { failing.execute({TaskOperation::EDIT, id, beforeFailure->localRevision, "Must roll back", {}}); });
      check(repository.find(id) == beforeFailure && count(database, "CommandRecord") == receiptCount
            && count(database, "LocalChange") == changeCount && !failing.canUndo(), "Transaction was partially persisted");
      // A duplicate receipt operation fails after the task write, proving real SQL-error rollback.
      auto duplicate = created;
      duplicate.before = repository.find(id);
      duplicate.after = *duplicate.before;
      ++duplicate.after.localRevision;
      ++duplicate.after.updatedAt;
      rejects([&]() { repository.commit(duplicate); });
      check(repository.find(id) == beforeFailure && count(database, "LocalChange") == changeCount, "Audit SQL failure leaked task write");
      auto changeFailure = duplicate;
      changeFailure.id = uuid();
      changeFailure.operationId = uuid();
      QSqlQuery collision(database);
      collision.prepare("UPDATE LocalChange SET id=? WHERE operationId=?");
      collision.addBindValue(QString::fromStdString(changeFailure.id));
      collision.addBindValue(QString::fromStdString(created.operationId));
      check(collision.exec() && collision.numRowsAffected() == 1, "Cannot arrange isolated outbox failure");
      rejects([&]() { repository.commit(changeFailure); });
      check(repository.find(id) == beforeFailure && count(database, "CommandRecord") == receiptCount
            && count(database, "LocalChange") == changeCount, "Outbox failure leaked task or audit write");
      QSqlQuery invalid(database);
      invalid.prepare("UPDATE Task SET lifecycleStatus='completed', completedAt=NULL WHERE id=?");
      invalid.addBindValue(QString::fromStdString(id));
      check(!invalid.exec(), "Database lifecycle CHECK was not enforced");
      rejects([&]() { service.execute({TaskOperation::EDIT, id, 1, "Stale", {}}); });
      service.undo();
      service.execute({TaskOperation::EDIT, id, repository.find(id)->localRevision, "New branch", ""});
      check(!service.canRedo(), "New edit retained redo branch");
      {
         // A genuinely new connection/session, not the existing in-memory objects.
         DatabaseConnection restarted(config);
         QSqlQuery competingLock(restarted.database());
         check(competingLock.exec("SELECT GET_LOCK('TaskCompassMigration:taskcompass_integration',0)")
               && competingLock.next() && competingLock.value(0).toInt() == 0, "Integration lease did not exclude a competing connection");
         MariaDbTaskRepository reloaded(restarted.database(), MigrationRunner(restarted.database()).verify());
         check(reloaded.find(id) == repository.find(id), "Restart lost data/timestamps/UUID");
         TaskService fresh(reloaded, dataset, [&now]() { return now; }, uuid);
         check(!fresh.canUndo() && fresh.list().size() == 1, "Session history survived restart unexpectedly");
      }
      execute(database, "ALTER TABLE Task ADD COLUMN unexpectedTestColumn INT NULL");
      rejects([&]() { migrations.verify(); });
      resetTestSchema(database, safety); // Only the explicitly disposable integration schema.
      QTextStream(stdout) << "PASS: MariaDB migrations/version/checksum/fingerprint, persistence/restart, commands, constraints and atomic rollback. Test schema left empty.\n";
      return 0;
   } catch (const std::exception &error) {
      QTextStream(stderr) << "FAILED: " << error.what() << "\nTest schema may contain evidence; no automatic cleanup on failure.\n";
      return 1;
   }
}
