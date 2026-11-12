#pragma once

#include <QString>

namespace taskcompass {

struct DatabaseConfig {
   QString host;
   int port = 3306;
   QString databaseName;
   QString user;
   QString password;
   void validate() const;
   void validateIdentity(const QString &selectedDatabase, const QString &authenticatedAccount) const;
   static DatabaseConfig fromEnvironment(bool integration = false);
};

}
