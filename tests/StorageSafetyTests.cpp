#include "IntegrationTestSafety.h"
#include "infrastructure/storage/DatabaseConnection.h"
#include "infrastructure/storage/MigrationRunner.h"

#include <QCoreApplication>
#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTextStream>
#include <stdexcept>

namespace {
void check(bool condition) {
   if (!condition) {
      throw std::runtime_error("Storage configuration guard failed");
   }
}
template<typename Operation> void rejects(Operation operation) {
   bool rejected = false;
   try { operation(); } catch (const std::exception &) { rejected = true; }
   check(rejected);
}
}

int main(int argc, char *argv[]) {
   QCoreApplication application(argc, argv);
   try {
      qputenv("TASKCOMPASS_DB_HOST", "127.0.0.1");
      qputenv("TASKCOMPASS_DB_PORT", "3306");
      qputenv("TASKCOMPASS_DB_NAME", "taskcompass_dev");
      qputenv("TASKCOMPASS_DB_USER", "taskcompass_app");
      qputenv("TASKCOMPASS_DB_PASSWORD", "fake-test-value");
      qputenv("TASKCOMPASS_DB_ISOLATED", "0");
      rejects([]() { taskcompass::DatabaseConfig::fromEnvironment(); });
      qputenv("TASKCOMPASS_DB_ISOLATED", "1");
      check(taskcompass::DatabaseConfig::fromEnvironment().databaseName == "taskcompass_dev");
      for (const auto &user : {"root", "taskcompass_dev", "unrelated"}) {
         qputenv("TASKCOMPASS_DB_USER", user);
         rejects([]() { taskcompass::DatabaseConfig::fromEnvironment(); });
      }
      qputenv("TASKCOMPASS_DB_USER", "taskcompass_app");
      qputenv("TASKCOMPASS_DB_NAME", "unrelated");
      rejects([]() { taskcompass::DatabaseConfig::fromEnvironment(); });
      qputenv("TASKCOMPASS_TEST_DB_HOST", "127.0.0.1");
      qputenv("TASKCOMPASS_TEST_DB_PORT", "3306");
      qputenv("TASKCOMPASS_TEST_DB_USER", "taskcompass_test");
      qputenv("TASKCOMPASS_TEST_DB_PASSWORD", "fake-test-value");
      qputenv("TASKCOMPASS_TEST_DB_ISOLATED", "1");
      qputenv("TASKCOMPASS_TEST_DB_NAME", "taskcompass_dev");
      rejects([]() { taskcompass::DatabaseConfig::fromEnvironment(true); });
      qputenv("TASKCOMPASS_TEST_DB_NAME", "taskcompass_integration");
      check(taskcompass::DatabaseConfig::fromEnvironment(true).databaseName == "taskcompass_integration");
      qputenv("TASKCOMPASS_TEST_DB_HOST", "remote.invalid");
      rejects([]() { taskcompass::DatabaseConfig::fromEnvironment(true); });
      qputenv("TASKCOMPASS_DB_NAME", "taskcompass_dev");
      qputenv("TASKCOMPASS_DB_PORT", "99999");
      rejects([]() { taskcompass::DatabaseConfig::fromEnvironment(); });
      if (application.arguments().size() != 3) {
         throw std::runtime_error("Expected migration source and integration executable paths");
      }
      QFile source(application.arguments().at(1));
      check(source.open(QIODevice::ReadOnly));
      check(source.readAll() == taskcompass::MigrationRunner::migrationSql());
      const taskcompass::DatabaseConfig isolated{"127.0.0.1", 3306, "taskcompass_integration", "taskcompass_test", "fake-test-value"};
      isolated.validate();
      isolated.validateIdentity("taskcompass_integration", "taskcompass_test@127.0.0.1");
      for (const auto &schema : {"taskcompass_dev", "unrelated", ""}) {
         rejects([&]() { isolated.validateIdentity(schema, "taskcompass_test@127.0.0.1"); });
         auto forbidden = isolated;
         forbidden.databaseName = schema;
         rejects([&]() { taskcompass::DatabaseConnection connection(forbidden); }); // Rejects before opening.
      }
      for (const auto &account : {"root@127.0.0.1", "taskcompass_test@localhost", "taskcompass_app@127.0.0.1", ""}) {
         rejects([&]() { isolated.validateIdentity("taskcompass_integration", account); });
      }
      auto forbiddenAccount = isolated;
      forbiddenAccount.user = "root";
      rejects([&]() { taskcompass::DatabaseConnection connection(forbiddenAccount); });
      taskcompass::MigrationRunner unopened{QSqlDatabase()};
      rejects([&]() { unopened.verify(); });
      rejects([&]() { unopened.applyInitial(); }); // Fails before any SQL on an invalid handle.

      IntegrationTestSafety::Catalog empty;
      IntegrationTestSafety::Catalog known;
      known.tables.append({"Task", "BASE TABLE", "InnoDB"});
      IntegrationTestSafety fresh;
      rejects([&]() { fresh.requireResetAllowed(empty); });
      rejects([&]() { fresh.begin(known, 0); }); // Recognized existing objects are still protected at startup.
      auto invalid = empty;
      invalid.tables.append({"Unrelated", "BASE TABLE", "InnoDB"});
      rejects([&]() { fresh.begin(invalid, 0); });
      for (int kind = 0; kind < 3; ++kind) {
         auto storedCode = empty;
         if (kind == 0) { storedCode.routines.append("unexpected"); }
         if (kind == 1) { storedCode.triggers.append("unexpected"); }
         if (kind == 2) { storedCode.events.append("unexpected"); }
         rejects([&]() { fresh.begin(storedCode, 0); });
      }
      fresh.begin(empty, 0);
      rejects([&]() { fresh.begin(empty, 0); });
      fresh.requireResetAllowed(empty);
      fresh.requireResetAllowed(known);
      rejects([&]() { fresh.requireResetAllowed(invalid); });
      invalid = known;
      invalid.tables[0].name = "task";
      rejects([&]() { fresh.requireResetAllowed(invalid); });
      invalid = known;
      invalid.tables[0].type = "VIEW";
      rejects([&]() { fresh.requireResetAllowed(invalid); });
      invalid = known;
      invalid.tables[0].engine = "MyISAM";
      rejects([&]() { fresh.requireResetAllowed(invalid); });
      for (int kind = 0; kind < 3; ++kind) {
         auto storedCode = known;
         if (kind == 0) { storedCode.routines.append("unexpected"); }
         if (kind == 1) { storedCode.triggers.append("unexpected"); }
         if (kind == 2) { storedCode.events.append("unexpected"); }
         rejects([&]() { fresh.requireResetAllowed(storedCode); });
      }
      IntegrationTestSafety windows;
      windows.begin(empty, 1);
      invalid = known;
      invalid.tables[0].name = "task";
      windows.requireResetAllowed(invalid); // Server-declared case-insensitive names only.
      IntegrationTestSafety invalidCase;
      rejects([&]() { invalidCase.begin(empty, -1); });
      rejects([&]() { invalidCase.begin(empty, 3); });

      auto environment = QProcessEnvironment::systemEnvironment();
      for (const auto &key : environment.keys()) {
         if (key.startsWith("TASKCOMPASS_")) {
            environment.remove(key);
         }
      }
      environment.insert("TASKCOMPASS_DB_HOST", "127.0.0.1");
      environment.insert("TASKCOMPASS_DB_NAME", "taskcompass_dev");
      environment.insert("TASKCOMPASS_DB_USER", "taskcompass_app");
      environment.insert("TASKCOMPASS_DB_PASSWORD", "fake-test-value");
      environment.insert("TASKCOMPASS_DB_ISOLATED", "1");
      auto runBlocked = [&](const QStringList &arguments, const QProcessEnvironment &processEnvironment, int exitCode,
                            const QByteArray &diagnostic) {
         QProcess process;
         process.setProcessEnvironment(processEnvironment);
         process.start(application.arguments().at(2), arguments);
         check(process.waitForFinished(10000) && process.exitStatus() == QProcess::NormalExit
               && process.exitCode() == exitCode);
         const auto output = process.readAllStandardOutput() + process.readAllStandardError();
         check(output.contains(diagnostic) && !output.contains("fake-test-value"));
      };
      for (const auto &arguments : QList<QStringList>{{}, {"--run"}, {"--allow-reset"}, {"--allow-reset", "--run"},
                                                     {"--run", "--allow-reset", "extra"}}) {
         runBlocked(arguments, environment, 77, "SKIPPED");
      }
      runBlocked({"--run", "--allow-reset"}, environment, 1, "Explicit isolated database configuration");
      environment.insert("TASKCOMPASS_TEST_DB_HOST", "127.0.0.1");
      environment.insert("TASKCOMPASS_TEST_DB_NAME", "taskcompass_dev");
      environment.insert("TASKCOMPASS_TEST_DB_USER", "taskcompass_test");
      environment.insert("TASKCOMPASS_TEST_DB_PASSWORD", "fake-test-value");
      environment.insert("TASKCOMPASS_TEST_DB_ISOLATED", "1");
      runBlocked({"--run", "--allow-reset"}, environment, 1, "Invalid isolated database/account configuration");
      environment.insert("TASKCOMPASS_TEST_DB_NAME", "unrelated");
      runBlocked({"--run", "--allow-reset"}, environment, 1, "Invalid isolated database/account configuration");
      environment.insert("TASKCOMPASS_TEST_DB_NAME", "taskcompass_integration");
      environment.insert("TASKCOMPASS_TEST_DB_USER", "root");
      runBlocked({"--run", "--allow-reset"}, environment, 1, "Invalid isolated database/account configuration");
      environment.insert("TASKCOMPASS_TEST_DB_USER", "taskcompass_test");
      environment.insert("TASKCOMPASS_TEST_DB_HOST", "remote.invalid");
      runBlocked({"--run", "--allow-reset"}, environment, 1, "Invalid isolated database/account configuration");
      QTextStream(stdout) << "PASS: configuration/authenticated-identity guards, fresh-schema/run-owned reset policy, fail-closed migrations, process opt-in/config rejection and embedded SQL equality; no live SQL or database connection attempted.\n";
      return 0;
   } catch (const std::exception &error) {
      QTextStream(stderr) << error.what() << '\n';
      return 1;
   }
}
