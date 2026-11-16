#include "MigrationRunner.h"
#include "DatabaseConfig.h"
#include "MutationLease.h"
#include "domain/Task.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QSqlQuery>
#include <QStringList>
#include <QUuid>
#include <QVariant>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <stdexcept>

// Explicit reference ensures a static library's resource object is linked.
static void initializeMigrations() {
   Q_INIT_RESOURCE(Migrations);
}

namespace taskcompass {
namespace {
const QStringList TABLES = {"SchemaMigration", "Dataset", "Task", "CommandRecord", "LocalChange"};

void require(bool condition, const char *message) {
   if (!condition) {
      throw std::runtime_error(message);
   }
}

QStringList tableNames(QSqlDatabase database) {
   QSqlQuery query(database);
   require(query.exec("SELECT TABLE_NAME FROM information_schema.TABLES WHERE TABLE_SCHEMA = DATABASE()"),
           "Cannot inspect schema tables.");
   QStringList result;
   while (query.next()) {
      result.append(query.value(0).toString().toLower());
   }
   result.sort();
   return result;
}
}

MigrationRunner::MigrationRunner(QSqlDatabase database, std::function<void(int)> afterStep)
   : database(database), afterStep(std::move(afterStep)) {}

void MigrationRunner::requireIdentity(bool writing) {
   require(database.isOpen() && database.driverName() == "QMYSQL", "Migration access requires an open QMYSQL connection to an approved TaskCompass database.");
   DatabaseConfig config{database.hostName(), database.port(), database.databaseName(), database.userName(), database.password()};
   config.validate();
   if (writing) {
      require(config.user == (config.databaseName == "taskcompass_phase12_test" ? "taskcompass_phase12_test"
                             : config.databaseName == "taskcompass_integration" ? "taskcompass_test" : "taskcompass_migrate"),
              "Migration writes require the dedicated migration/test account, never runtime or administrator credentials.");
   }
   QSqlQuery identity(database);
   require(identity.exec("SELECT DATABASE(), CURRENT_USER(), @@foreign_key_checks, @@check_constraint_checks") && identity.next(), "Cannot verify migration database/account identity.");
   config.validateIdentity(identity.value(0).toString(), identity.value(1).toString());
   require(!writing || (identity.value(2).toInt() == 1 && identity.value(3).toInt() == 1),
           "Migration writes require foreign-key and CHECK enforcement enabled.");
}

QByteArray MigrationRunner::migrationSql(int version) {
   initializeMigrations();
   require(version == 1 || version == 2, "Unknown migration version.");
   QFile file(version == 1 ? ":/migrations/001.sql" : ":/migrations/002.sql");
   require(file.open(QIODevice::ReadOnly), "Embedded migration SQL is unavailable.");
   return file.readAll();
}

QStringList MigrationRunner::tableNamesForVersion(int version) {
   require(version == 1 || version == 2, "Unsupported schema version.");
   auto result = TABLES;
   if (version == 2) { result.append({"MigrationAttempt", "Pillar", "Project", "Objective"}); }
   return result;
}

QString MigrationRunner::fingerprint(int version) {
   QByteArray schema;
   const auto tables = tableNamesForVersion(version);
   for (const auto &table : tables) {
      QSqlQuery query(database);
      require(query.exec("SHOW CREATE TABLE `" + table + "`") && query.next(),
              "Missing/inaccessible migration table. Verify schema; do not rerun partial DDL.");
      auto ddl = query.value(1).toString();
      // Windows lower_case_table_names can normalize table identifiers.
      for (const auto &name : tables) {
         ddl.replace("`" + name + "`", "`" + name.toLower() + "`", Qt::CaseInsensitive);
      }
      schema += ddl.toUtf8() + '\n';
   }
   return QString::fromLatin1(QCryptographicHash::hash(schema, QCryptographicHash::Sha256).toHex());
}

void MigrationRunner::requireShape(int version) {
   auto expected = tableNamesForVersion(version);
   for (auto &name : expected) {
      name = name.toLower();
   }
   expected.sort();
   require(tableNames(database) == expected,
           "Uninitialized, partial, or unexpected schema. Use the reviewed migration/recovery procedure.");
   QSqlQuery objects(database);
   require(objects.exec("SELECT TABLE_TYPE,ENGINE FROM information_schema.TABLES WHERE TABLE_SCHEMA=DATABASE()"), "Cannot inspect table engines.");
   while (objects.next()) { require(objects.value(0).toString() == "BASE TABLE" && objects.value(1).toString().compare("InnoDB", Qt::CaseInsensitive) == 0, "Unexpected schema object/engine."); }
   for (const auto &catalog : {"ROUTINES", "TRIGGERS", "EVENTS"}) {
      const QString schema = QString(catalog) == "ROUTINES" ? "ROUTINE_SCHEMA" : QString(catalog) == "TRIGGERS" ? "TRIGGER_SCHEMA" : "EVENT_SCHEMA";
      require(objects.exec("SELECT COUNT(*) FROM information_schema." + QString(catalog) + " WHERE " + schema + "=DATABASE()")
              && objects.next() && objects.value(0).toInt() == 0, "Unexpected/inaccessible stored code; operator review required.");
   }
   initializeMigrations();
   QFile manifest(":/migrations/schema.json");
   require(manifest.open(QIODevice::ReadOnly), "Missing structural migration registry.");
   const auto expectedTables = QJsonDocument::fromJson(manifest.readAll()).object()[QString::number(version)].toObject();
   require(expectedTables.size() == expected.size(), "Invalid structural migration registry.");
   for (auto entry = expectedTables.begin(); entry != expectedTables.end(); ++entry) {
      const auto table = entry.key();
      const auto descriptor = entry.value().toObject();
      const auto expectedColumns = descriptor["columns"].toArray();
      QSqlQuery columns(database);
      columns.prepare("SELECT COLUMN_NAME,COLUMN_TYPE,IS_NULLABLE,CHARACTER_SET_NAME,COLLATION_NAME FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND LOWER(TABLE_NAME)=LOWER(?) ORDER BY ORDINAL_POSITION");
      columns.addBindValue(table);
      require(columns.exec(), "Cannot verify migration column structure.");
      for (const auto &value : expectedColumns) {
         const auto column = value.toObject();
         require(columns.next() && columns.value(0).toString() == column["name"].toString(), "Schema columns differ from registered migration.");
         auto actualType = columns.value(1).toString().toLower();
         actualType.replace(QRegularExpression("^(bigint|int)\\([0-9]+\\)$"), "\\1");
         require(actualType == column["type"].toString().toLower()
                 && (columns.value(2).toString() == "YES") == column["nullable"].toBool(), "Schema type/nullability differs from registered migration.");
         const auto type = column["type"].toString();
         if (type.startsWith("CHAR") || type.startsWith("VARCHAR") || type == "MEDIUMTEXT") {
            const bool ascii = column["modifiers"].toString().contains("CHARACTER SET ascii");
            require(columns.value(3).toString() == (ascii ? "ascii" : "utf8mb4") && columns.value(4).toString() == (ascii ? "ascii_bin" : "utf8mb4_unicode_ci"), "Schema text encoding differs from registered migration.");
         }
      }
      require(!columns.next(), "Unexpected schema column.");
      auto namesOfConstraints = [&](const QString &kind) {
         QSqlQuery query(database);
         query.prepare("SELECT CONSTRAINT_NAME FROM information_schema.TABLE_CONSTRAINTS WHERE CONSTRAINT_SCHEMA=DATABASE() AND LOWER(TABLE_NAME)=LOWER(?) AND CONSTRAINT_TYPE=? ORDER BY CONSTRAINT_NAME");
         query.addBindValue(table); query.addBindValue(kind);
         require(query.exec(), "Cannot inspect schema constraints.");
         QStringList names;
         while (query.next()) { names.append(query.value(0).toString()); }
         names.sort(); return names;
      };
      QStringList expectedChecks;
      for (const auto &check : descriptor["checks"].toArray()) { expectedChecks.append(check.toArray()[0].toString()); }
      expectedChecks.sort();
      require(namesOfConstraints("CHECK") == expectedChecks, "CHECK constraints differ from registered migration.");
      QStringList expectedForeignKeys;
      for (const auto &value : descriptor["foreignKeys"].toArray()) {
         const auto key = value.toArray(); expectedForeignKeys.append(key[0].toString());
         QSqlQuery foreign(database);
         foreign.prepare("SELECT COLUMN_NAME,REFERENCED_TABLE_SCHEMA,REFERENCED_TABLE_NAME,REFERENCED_COLUMN_NAME FROM information_schema.KEY_COLUMN_USAGE WHERE CONSTRAINT_SCHEMA=DATABASE() AND LOWER(TABLE_NAME)=LOWER(?) AND CONSTRAINT_NAME=? ORDER BY ORDINAL_POSITION");
         foreign.addBindValue(table); foreign.addBindValue(key[0].toString());
         require(foreign.exec(), "Cannot inspect foreign key columns.");
         const auto children = key[1].toArray(), parents = key[3].toArray();
         for (int part = 0; part < children.size(); ++part) {
            require(foreign.next() && foreign.value(0).toString() == children[part].toString()
                    && foreign.value(1).toString() == database.databaseName()
                    && foreign.value(2).toString().compare(key[2].toString(), Qt::CaseInsensitive) == 0
                    && foreign.value(3).toString() == parents[part].toString(), "Foreign key differs from registered migration.");
         }
         require(!foreign.next(), "Unexpected foreign key column.");
      }
      expectedForeignKeys.sort();
      require(namesOfConstraints("FOREIGN KEY") == expectedForeignKeys, "Unexpected or missing foreign key constraint.");
      auto verifyIndex = [&](const QString &name, const QJsonArray &expectedColumns, bool unique) {
         QSqlQuery index(database);
         index.prepare("SELECT COLUMN_NAME,NON_UNIQUE FROM information_schema.STATISTICS WHERE TABLE_SCHEMA=DATABASE() AND LOWER(TABLE_NAME)=LOWER(?) AND INDEX_NAME=? ORDER BY SEQ_IN_INDEX");
         index.addBindValue(table); index.addBindValue(name);
         require(index.exec(), "Cannot inspect migration index.");
         for (const auto &column : expectedColumns) {
            require(index.next() && index.value(0).toString() == column.toString()
                    && index.value(1).toInt() == (unique ? 0 : 1), "Index columns/uniqueness differ from registered migration.");
         }
         require(!index.next(), "Unexpected index column.");
      };
      QJsonArray primary;
      for (const auto &column : expectedColumns) {
         const auto object = column.toObject();
         if (object["keys"].toArray().contains("PK")) { primary.append(object["name"]); }
      }
      verifyIndex("PRIMARY", primary, true);
      for (const auto &key : descriptor["uniqueKeys"].toArray()) { const auto parts = key.toArray(); verifyIndex(parts[0].toString(), parts[1].toArray(), true); }
      for (const auto &key : descriptor["indexes"].toArray()) { const auto parts = key.toArray(); verifyIndex(parts[0].toString(), parts[1].toArray(), false); }
   }
}

std::string MigrationRunner::verify(int requiredVersion) {
   requireIdentity(false);
   QSqlQuery history(database);
   require(history.exec("SELECT version,checksum,schemaFingerprint,appliedAt FROM SchemaMigration ORDER BY version"), "No completed migration recorded. Partial initialization requires operator recovery.");
   int current = 0;
   QString baseline;
   QString latest;
   while (history.next()) {
      ++current;
      require(current <= 2 && history.value(0).toInt() == current, "Unsupported/gapped/newer schema history; use a compatible TaskCompass build.");
      require(history.value(1).toByteArray() == QCryptographicHash::hash(migrationSql(current), QCryptographicHash::Sha256).toHex(), "Migration checksum mismatch; do not edit applied migrations.");
      require(history.value(3).toLongLong() > 0, "Invalid migration timestamp.");
      latest = history.value(2).toString();
      if (current == 1) { baseline = latest; }
   }
   require(current > 0 && (requiredVersion == 0 || current == requiredVersion), "Required schema version is unavailable; request the reviewed migration, never auto-upgrade.");
   requireShape(current);
   require(latest == fingerprint(current), "Schema fingerprint changed; operator review required.");
   if (current == 2) {
      QSqlQuery attempt(database);
      require(attempt.exec("SELECT version,checksum,baselineFingerprint,startedAt,completedAt,lastCompletedStep,attemptState FROM MigrationAttempt ORDER BY version") && attempt.next(), "Missing migration attempt; explicit review required.");
      require(attempt.value(0).toInt() == 2 && attempt.value(1).toByteArray() == QCryptographicHash::hash(migrationSql(2), QCryptographicHash::Sha256).toHex()
              && attempt.value(2).toString() == baseline && attempt.value(3).toLongLong() > 0 && !attempt.value(4).isNull()
              && attempt.value(4).toLongLong() >= attempt.value(3).toLongLong() && attempt.value(5).toInt() == 9
              && attempt.value(6).toString() == "completed" && !attempt.next(), "Interrupted/inconsistent migration attempt; explicit review required, no resume/repair.");
   }
   QSqlQuery dataset(database);
   require(dataset.exec("SELECT id FROM Dataset") && dataset.next(), "Dataset is missing.");
   const auto id = dataset.value(0).toString().toStdString();
   require(isUuid(id) && !dataset.next(), "Exactly one canonical dataset is required in Phase 1.1.");
   currentVersion = current;
   return id;
}

void MigrationRunner::applyInitial() {
   requireIdentity(true);
   QSqlQuery lock(database);
   require(lock.exec("SELECT GET_LOCK(CONCAT('TaskCompassMigration:', DATABASE()), 0)")
           && lock.next() && lock.value(0).toInt() == 1, "Migration is locked by another process.");
   struct LockRelease {
      QSqlDatabase database;
      ~LockRelease() {
         QSqlQuery query(database);
         query.exec("SELECT RELEASE_LOCK(CONCAT('TaskCompassMigration:', DATABASE()))");
      }
   } release{database};
   const auto tables = tableNames(database);
   if (!tables.isEmpty()) {
      verify(1); // Legacy initializer only; never auto-resume or alter populated schemas.
      return;
   }
   for (const auto &catalog : {"ROUTINES", "TRIGGERS", "EVENTS"}) {
      const QString schema = QString(catalog) == "ROUTINES" ? "ROUTINE_SCHEMA" : QString(catalog) == "TRIGGERS" ? "TRIGGER_SCHEMA" : "EVENT_SCHEMA";
      QSqlQuery objects(database);
      require(objects.exec("SELECT COUNT(*) FROM information_schema." + QString(catalog) + " WHERE " + schema + "=DATABASE()")
              && objects.next() && objects.value(0).toInt() == 0, "Fresh migration target contains unexpected/inaccessible stored code; no DDL submitted.");
   }
   const auto sql = migrationSql();
   // Migration 001 contains simple statements, no quoted semicolons or stored programs.
   for (const auto &statement : QString::fromUtf8(sql).split(';', Qt::SkipEmptyParts)) {
      if (statement.trimmed().isEmpty()) {
         continue;
      }
      QSqlQuery query(database);
      require(query.exec(statement),
              "Migration DDL failed and may be partially committed. Stop; preserve evidence and follow recovery instructions.");
   }
   const auto schemaFingerprint = fingerprint();
   require(database.transaction(), "Cannot start migration metadata transaction; DDL may already exist.");
   try {
      QSqlQuery dataset(database);
      dataset.prepare("INSERT INTO Dataset (id, name, createdAt) VALUES (?, ?, ?)");
      dataset.addBindValue(QUuid::createUuid().toString(QUuid::WithoutBraces));
      dataset.addBindValue("Local tasks");
      dataset.addBindValue(QDateTime::currentMSecsSinceEpoch());
      require(dataset.exec(), "Cannot initialize dataset; transaction rolled back, DDL remains.");
      QSqlQuery version(database);
      version.prepare("INSERT INTO SchemaMigration (version, checksum, schemaFingerprint, appliedAt) VALUES (1, ?, ?, ?)");
      version.addBindValue(QString::fromLatin1(QCryptographicHash::hash(sql, QCryptographicHash::Sha256).toHex()));
      version.addBindValue(schemaFingerprint);
      version.addBindValue(QDateTime::currentMSecsSinceEpoch());
      require(version.exec(), "Cannot record migration; transaction rolled back, DDL remains.");
      require(database.commit(), "Migration metadata commit failed; inspect database before retrying.");
   } catch (...) {
      database.rollback();
      throw;
   }
   verify(1);
}

QByteArray MigrationRunner::legacyDataDigest() {
   const QList<QPair<QString, QString>> projections = {
      {"Dataset", "id,name,createdAt"},
      {"Task", "id,datasetId,title,notes,lifecycleStatus,createdAt,updatedAt,completedAt,deletedAt,localRevision"},
      {"CommandRecord", "id,operationId,datasetId,taskId,commandType,payloadVersion,recordedAt,beforeSnapshot,afterSnapshot,resultingRevision,compensatesCommandId,reappliesCommandId"},
      {"LocalChange", "id,operationId,datasetId,taskId,localRevision,payloadVersion,immutablePayload,createdAt,acknowledgmentState"},
      {"SchemaMigration", "version,checksum,schemaFingerprint,appliedAt"}};
   QCryptographicHash hash(QCryptographicHash::Sha256);
   for (const auto &[table, columns] : projections) {
      QSqlQuery query(database);
      require(query.exec("SELECT " + columns + " FROM " + table + (table == "SchemaMigration" ? " WHERE version=1 ORDER BY version" : " ORDER BY id")), "Cannot verify original-data preservation.");
      hash.addData(table.toUtf8());
      while (query.next()) {
         for (int column = 0; column < columns.count(',') + 1; ++column) {
            if (query.value(column).isNull()) { hash.addData(QByteArray("N;")); }
            else {
               const auto bytes = query.value(column).toString().toUtf8();
               hash.addData(QByteArray::number(bytes.size()) + ':' + bytes + ';');
            }
         }
      }
   }
   return hash.result();
}

void MigrationRunner::applyToVersion(int target) {
   require(target == 1 || target == 2, "Unknown migration target.");
   if (target == 1) { applyInitial(); return; }
   requireIdentity(true);
   QSqlQuery lock(database);
   require(lock.exec("SELECT GET_LOCK(CONCAT('TaskCompassMigration:',DATABASE()),0)") && lock.next()
           && lock.value(0).toInt() == 1, "Another migration owns this schema.");
   struct Release {
      QSqlDatabase database;
      ~Release() { QSqlQuery query(database); query.exec("SELECT RELEASE_LOCK(CONCAT('TaskCompassMigration:',DATABASE()))"); }
   } release{database};
   MutationLease mutation(database, 0);
   if (tableNames(database).isEmpty()) { applyInitial(); }
   auto names = tableNames(database);
   if (names.contains("migrationattempt")) {
      verify(2); // Complete only; partial bootstrap/failed attempts refuse without any DDL.
      return;
   }
   verify(1);
   const auto beforeData = legacyDataDigest();
   const auto baseline = fingerprint(1);
   const auto sql = migrationSql(2);
   const auto checksum = QCryptographicHash::hash(sql, QCryptographicHash::Sha256).toHex();
   const auto statements = QString::fromUtf8(sql).split(';', Qt::SkipEmptyParts);
   require(statements.size() == 9, "Migration 002 statement manifest changed; explicit review required.");
   const auto startedAt = QDateTime::currentMSecsSinceEpoch();
   bool attempted = false;
   bool committingMetadata = false;
   try {
      for (int step = 0; step < statements.size(); ++step) {
         QSqlQuery query(database);
         require(query.exec(statements[step]), "Migration 002 DDL failed; explicit review required. Preserve partial state; no resume/reset/repair.");
         if (step == 0) {
            if (afterStep) { afterStep(0); } // Bootstrap interruption before journal INSERT.
            query.prepare("INSERT INTO MigrationAttempt (version,checksum,baselineFingerprint,startedAt,completedAt,lastCompletedStep,attemptState) VALUES (2,?,?,?,NULL,1,'running')");
            query.addBindValue(QString::fromLatin1(checksum)); query.addBindValue(baseline); query.addBindValue(startedAt);
            require(query.exec(), "Migration journal bootstrap incomplete; explicit review required.");
            attempted = true;
         } else {
            query.prepare("UPDATE MigrationAttempt SET lastCompletedStep=? WHERE version=2 AND checksum=? AND attemptState='running' AND lastCompletedStep=?");
            query.addBindValue(step + 1); query.addBindValue(QString::fromLatin1(checksum)); query.addBindValue(step);
            require(query.exec() && query.numRowsAffected() == 1, "Migration journal acknowledgment failed; inspect actual DDL, never resume blindly.");
         }
         if (afterStep) { afterStep(step + 1); }
      }
      requireShape(2);
      require(legacyDataDigest() == beforeData, "Original schema-1 data changed; stop and preserve evidence.");
      const auto afterFingerprint = fingerprint(2);
      require(database.transaction(), "Cannot start final migration metadata transaction.");
      QSqlQuery query(database);
      query.prepare("INSERT INTO SchemaMigration (version,checksum,schemaFingerprint,appliedAt) VALUES (2,?,?,?)");
      query.addBindValue(QString::fromLatin1(checksum)); query.addBindValue(afterFingerprint); query.addBindValue(startedAt);
      require(query.exec(), "Cannot record schema 2; explicit review required.");
      if (afterStep) { afterStep(10); }
      query.prepare("UPDATE MigrationAttempt SET attemptState='completed',completedAt=? WHERE version=2 AND checksum=? AND attemptState='running' AND lastCompletedStep=9");
      query.addBindValue(std::max(startedAt, QDateTime::currentMSecsSinceEpoch())); query.addBindValue(QString::fromLatin1(checksum));
      require(query.exec() && query.numRowsAffected() == 1, "Cannot complete migration journal.");
      if (afterStep) { afterStep(11); }
      committingMetadata = true;
      require(database.commit(), "Migration metadata commit outcome unknown. Inspect; never repeat automatically.");
      verify(2);
   } catch (...) {
      database.rollback();
      if (attempted && !committingMetadata && database.isOpen()) {
         QSqlQuery failed(database);
         failed.prepare("UPDATE MigrationAttempt SET attemptState='failed' WHERE version=2 AND checksum=? AND attemptState='running'");
         failed.addBindValue(QString::fromLatin1(checksum)); failed.exec();
      }
      throw;
   }
}

}
