#pragma once

#include "DatabaseConfig.h"

#include <QSqlDatabase>
#include <QString>

namespace taskcompass {

class DatabaseConnection {
public:
   explicit DatabaseConnection(const DatabaseConfig &config);
   ~DatabaseConnection();
   DatabaseConnection(const DatabaseConnection &) = delete;
   DatabaseConnection &operator=(const DatabaseConnection &) = delete;
   QSqlDatabase &database();

private:
   QString connectionName;
   QSqlDatabase connection;
};

}
