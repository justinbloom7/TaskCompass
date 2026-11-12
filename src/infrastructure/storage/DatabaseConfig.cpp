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
   const QString expectedDatabase = integration ? "taskcompass_integration" : "taskcompass_dev";
   if (environment.value(prefix + "ISOLATED") != "1" || config.host != "127.0.0.1"
       || config.databaseName != expectedDatabase
       || (integration ? config.user != "taskcompass_test"
                       : (config.user != "taskcompass_app" && config.user != "taskcompass_migrate"))
       || config.password.isEmpty() || !validPort || config.port < 1 || config.port > 65535) {
      throw std::runtime_error(("Explicit isolated loopback configuration required: " + prefix
                               + "HOST/PORT/NAME/USER/PASSWORD/ISOLATED; database must be "
                               + expectedDatabase + ". See docs/Building.md.").toStdString());
   }
   return config;
}

}
