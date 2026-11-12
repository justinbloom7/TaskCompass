#include "infrastructure/storage/DatabaseConnection.h"
#include "infrastructure/storage/MigrationRunner.h"

#include <QCoreApplication>
#include <QFile>
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
      if (application.arguments().size() != 2) {
         throw std::runtime_error("Expected migration source path");
      }
      QFile source(application.arguments().at(1));
      check(source.open(QIODevice::ReadOnly));
      check(source.readAll() == taskcompass::MigrationRunner::migrationSql());
      QTextStream(stdout) << "PASS: isolated configuration/account guards and embedded/source migration equality; no database connection attempted.\n";
      return 0;
   } catch (const std::exception &error) {
      QTextStream(stderr) << error.what() << '\n';
      return 1;
   }
}
