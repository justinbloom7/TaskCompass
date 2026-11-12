#include "DatabaseConnection.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <stdexcept>

namespace taskcompass {

DatabaseConnection::DatabaseConnection(const DatabaseConfig &config)
   : connectionName(QUuid::createUuid().toString(QUuid::WithoutBraces)) {
   connection = QSqlDatabase::addDatabase("QMYSQL", connectionName);
   connection.setHostName(config.host);
   connection.setPort(config.port);
   connection.setDatabaseName(config.databaseName);
   connection.setUserName(config.user);
   connection.setPassword(config.password);
   connection.setConnectOptions("MYSQL_OPT_CONNECT_TIMEOUT=5;MYSQL_OPT_READ_TIMEOUT=5;MYSQL_OPT_WRITE_TIMEOUT=5");
   auto reject = [this](const char *message) {
      connection.close();
      connection = QSqlDatabase();
      QSqlDatabase::removeDatabase(connectionName);
      throw std::runtime_error(message);
   };
   if (!connection.open()) {
      // Do not expose connector error text: it may contain credentials/connection data.
      reject("Cannot open MariaDB. Check service/TCP availability, QMYSQL, account, password and grants.");
   }
   QSqlQuery query(connection);
   if (!query.exec("SET NAMES utf8mb4 COLLATE utf8mb4_unicode_ci")
       || !query.exec("SET SESSION time_zone = '+00:00'")
       || !query.exec("SET SESSION sql_quote_show_create = 1")
       || !query.exec("SET SESSION check_constraint_checks = ON")
       || !query.exec("SET SESSION sql_mode = 'STRICT_ALL_TABLES,NO_ENGINE_SUBSTITUTION'")) {
      query.finish();
      query = QSqlQuery();
      reject("Cannot establish strict utf8mb4/UTC MariaDB session.");
   }
}

DatabaseConnection::~DatabaseConnection() {
   connection.close();
   connection = QSqlDatabase();
   QSqlDatabase::removeDatabase(connectionName);
}

QSqlDatabase &DatabaseConnection::database() { return connection; }

}
