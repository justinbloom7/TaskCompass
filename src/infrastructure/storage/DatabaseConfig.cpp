#include "DatabaseConfig.h"

#include <QProcessEnvironment>
#include <stdexcept>

namespace taskcompass {

DatabaseConfig DatabaseConfig::fromEnvironment(bool integration) {
   const auto environment = QProcessEnvironment::systemEnvironment();
   const QString prefix = integration ? "TASKCOMPASS_TEST_DB_" : "TASKCOMPASS_DB_";
   DatabaseConfig config;
   config.host = environment.value(prefix + "HOST");
   config.databaseName = environment.value(prefix + "NAME");
   config.user = environment.value(prefix + "USER");
   config.password = environment.value(prefix + "PASSWORD");
   bool validPort = false;
   config.port = environment.value(prefix + "PORT", "3306").toInt(&validPort);
   if (environment.value(prefix + "ISOLATED") != "1" || !validPort) {
      throw std::runtime_error("Explicit isolated database configuration and a valid port are required. See docs/Building.md.");
   }
   config.validate();
   return config;
}

DatabaseConfig DatabaseConfig::fromPhase12Environment() {
   const auto environment = QProcessEnvironment::systemEnvironment();
   const QString prefix = "TASKCOMPASS_PHASE12_TEST_DB_";
   bool validPort = false;
   DatabaseConfig config{environment.value(prefix + "HOST"), environment.value(prefix + "PORT", "3306").toInt(&validPort),
                         environment.value(prefix + "NAME"), environment.value(prefix + "USER"), environment.value(prefix + "PASSWORD")};
   if (!validPort || environment.value(prefix + "ISOLATED") != "1"
       || config.databaseName != "taskcompass_phase12_test" || config.user != "taskcompass_phase12_test" || config.port != 3306) {
      throw std::runtime_error("Explicit isolated Phase 1.2 test database/account configuration is required.");
   }
   config.validate();
   return config;
}

DatabaseConfig DatabaseConfig::fromPhase13Environment() {
   const auto environment = QProcessEnvironment::systemEnvironment();
   const QString prefix = "TASKCOMPASS_PHASE13_TEST_DB_";
   bool validPort = false;
   DatabaseConfig config{environment.value(prefix + "HOST"), environment.value(prefix + "PORT").toInt(&validPort),
                         environment.value(prefix + "NAME"), environment.value(prefix + "USER"), environment.value(prefix + "PASSWORD")};
   if (!validPort || environment.value(prefix + "ISOLATED") != "1"
       || config.databaseName != "taskcompass_phase13_test" || config.user != "taskcompass_phase13_test") {
      throw std::runtime_error("Explicit isolated Phase 1.3 test database/account configuration is required.");
   }
   config.validate();
   return config;
}

void DatabaseConfig::validate() const {
   const bool integration = databaseName == "taskcompass_integration";
   const bool phase12 = databaseName == "taskcompass_phase12_test";
   const bool phase13 = databaseName == "taskcompass_phase13_test";
   const bool allowedAccount = phase13 ? (user == "taskcompass_phase13_test" || user == "taskcompass_phase13_app")
      : phase12 ? (user == "taskcompass_phase12_test" && port == 3306)
      : integration ? user == "taskcompass_test"
      : databaseName == "taskcompass_dev" && (user == "taskcompass_app" || user == "taskcompass_migrate");
   if (host != "127.0.0.1" || port < 1 || port > 65535 || password.isEmpty() || !allowedAccount) {
      throw std::runtime_error("Invalid isolated database/account configuration. Only approved loopback TaskCompass pairs are accepted.");
   }
}

void DatabaseConfig::validateIdentity(const QString &selectedDatabase, const QString &authenticatedAccount) const {
   validate();
   if (selectedDatabase != databaseName || authenticatedAccount != user + "@127.0.0.1") {
      throw std::runtime_error("Selected database or authenticated account differs from the explicit configuration; refusing access.");
   }
}

}
