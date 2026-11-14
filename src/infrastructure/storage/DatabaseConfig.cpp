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

void DatabaseConfig::validate() const {
   const bool integration = databaseName == "taskcompass_integration";
   if (host != "127.0.0.1" || port < 1 || port > 65535 || password.isEmpty()
       || (integration ? user != "taskcompass_test"
                       : (databaseName != "taskcompass_dev"
                          || (user != "taskcompass_app" && user != "taskcompass_migrate")))) {
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
