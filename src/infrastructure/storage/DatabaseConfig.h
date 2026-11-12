#pragma once

#include <QString>

namespace taskcompass {

struct DatabaseConfig {
   QString host;
   int port = 3306;
   QString databaseName;
   QString user;
   QString password;
   static DatabaseConfig fromEnvironment(bool integration = false);
};

}
