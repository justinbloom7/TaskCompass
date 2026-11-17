#pragma once

#include <QByteArray>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>
#include <string>
#include <functional>

namespace taskcompass {

class MigrationRunner {
public:
   explicit MigrationRunner(QSqlDatabase database, std::function<void(int)> afterStep = {});
   // Explicit caller consent is enforced by the separate CLI; never called by GUI.
   void applyInitial();
   void applyToVersion(int version);
   std::string verify(int requiredVersion = 0); // Read-only; returns the singleton dataset UUID.
   static QByteArray migrationSql(int version = 1);
   static QStringList migrationStatements(int version);
   static QStringList tableNamesForVersion(int version);
   int verifiedVersion() const { return currentVersion; }

private:
   void requireIdentity(bool writing);
   QString fingerprint(int version = 1);
   void requireShape(int version);
   QByteArray legacyDataDigest();
   QSqlDatabase database;
   int currentVersion = 0;
   std::function<void(int)> afterStep;
};

}
