#pragma once

#include <QByteArray>
#include <QSqlDatabase>
#include <QString>
#include <string>

namespace taskcompass {

class MigrationRunner {
public:
   explicit MigrationRunner(QSqlDatabase database);
   // Explicit caller consent is enforced by the separate CLI; never called by GUI.
   void applyInitial();
   std::string verify(); // Read-only; returns the singleton dataset UUID.
   static QByteArray migrationSql();

private:
   void requireIdentity(bool writing);
   QString fingerprint();
   QSqlDatabase database;
};

}
