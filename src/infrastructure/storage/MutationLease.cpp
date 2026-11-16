#include "MutationLease.h"
#include "DatabaseConfig.h"
#include <QSqlQuery>
#include <QVariant>
#include <stdexcept>

namespace taskcompass {

MutationLease::MutationLease(QSqlDatabase database, int timeoutSeconds) : database(database) {
   DatabaseConfig config{database.hostName(), database.port(), database.databaseName(), database.userName(), database.password()};
   config.validate();
   if (!database.isOpen() || database.driverName() != "QMYSQL" || timeoutSeconds < 0 || timeoutSeconds > 30) {
      throw std::runtime_error("Mutation lease requires an open approved QMYSQL connection and bounded timeout.");
   }
   QSqlQuery query(database);
   if (!query.exec("SELECT DATABASE(),CURRENT_USER(),@@foreign_key_checks,@@check_constraint_checks") || !query.next()) { throw std::runtime_error("Cannot verify mutation lease identity."); }
   config.validateIdentity(query.value(0).toString(), query.value(1).toString());
   if (query.value(2).toInt() != 1 || query.value(3).toInt() != 1) { throw std::runtime_error("Mutation lease requires FK and CHECK enforcement enabled."); }
   query.finish();
   name = "TaskCompassMutation:" + database.databaseName();
   query.prepare("SELECT GET_LOCK(?,?)");
   query.addBindValue(name); query.addBindValue(timeoutSeconds);
   if (!query.exec() || !query.next() || query.value(0).isNull() || query.value(0).toInt() != 1) {
      throw std::runtime_error("Workspace is busy or connection was lost; mutation lease not acquired. Reload before retrying.");
   }
}
MutationLease::~MutationLease() {
   if (database.isOpen()) {
      QSqlQuery query(database);
      query.prepare("SELECT RELEASE_LOCK(?)"); query.addBindValue(name); query.exec();
   }
}

}
