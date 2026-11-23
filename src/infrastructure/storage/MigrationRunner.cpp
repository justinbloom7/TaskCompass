#include "MigrationRunner.h"
#include "domain/FractionalRank.h"
#include "DatabaseConfig.h"
#include "MutationLease.h"
#include "SchemaConstraints.h"
#include "domain/Task.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QSqlQuery>
#include <QSqlError>
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
      require(config.user == (config.databaseName == "taskcompass_phase13_test" ? "taskcompass_phase13_test"
                             : config.databaseName == "taskcompass_phase12_test" ? "taskcompass_phase12_test"
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
   require(version >= 1 && version <= 3, "Unknown migration version.");
   QFile file(QString(":/migrations/00%1.sql").arg(version));
   require(file.open(QIODevice::ReadOnly), "Embedded migration SQL is unavailable.");
   return file.readAll();
}

QStringList MigrationRunner::migrationStatements(int version) {
   // These reviewed migrations contain simple DDL, no quoted semicolons/programs.
   // SkipEmptyParts alone retains a whitespace-only suffix after the final ';'.
   QStringList result;
   for (const auto &part : QString::fromUtf8(migrationSql(version)).split(';', Qt::SkipEmptyParts)) {
      const auto statement = part.trimmed();
      if (!statement.isEmpty()) { result.append(statement); }
   }
   return result;
}

QStringList MigrationRunner::tableNamesForVersion(int version) {
   require(version >= 1 && version <= 3, "Unsupported schema version.");
   auto result = TABLES;
   if (version >= 2) { result.append({"MigrationAttempt", "Pillar", "Project", "Objective"}); }
   if (version == 3) { result.append({"Context", "TaskContext", "WorkEntry", "TaskDayDisposition"}); }
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
   QFile manifest(version == 3 ? ":/migrations/schema3.json" : ":/migrations/schema.json");
   require(manifest.open(QIODevice::ReadOnly), "Missing structural migration registry.");
   const auto expectedTables = QJsonDocument::fromJson(manifest.readAll()).object()[QString::number(version)].toObject();
   require(expectedTables.size() == expected.size(), "Invalid structural migration registry.");
   for (auto entry = expectedTables.begin(); entry != expectedTables.end(); ++entry) {
      const auto table = entry.key();
      const auto descriptor = entry.value().toObject();
      const auto expectedColumns = descriptor["columns"].toArray();
      QSqlQuery columns(database);
      columns.prepare("SELECT COLUMN_NAME,COLUMN_TYPE,IS_NULLABLE,CHARACTER_SET_NAME,COLLATION_NAME,EXTRA,GENERATION_EXPRESSION FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND LOWER(TABLE_NAME)=LOWER(?) ORDER BY ORDINAL_POSITION");
      columns.addBindValue(table);
      require(columns.exec(), "Cannot verify migration column structure.");
      for (const auto &value : expectedColumns) {
         const auto column = value.toObject();
         require(columns.next() && columns.value(0).toString() == column["name"].toString(), "Schema columns differ from registered migration.");
         auto actualType = columns.value(1).toString().toLower();
         actualType.replace(QRegularExpression("^(bigint|int)\\([0-9]+\\)$"), "\\1");
         require(actualType == column["type"].toString().toLower()
                 && (columns.value(2).toString() == "YES") == column["nullable"].toBool(), "Schema type/nullability differs from registered migration.");
         if (version == 3 && column["modifiers"].toString().contains("GENERATED ALWAYS AS")) {
            require(columns.value(5).toString().contains("STORED GENERATED", Qt::CaseInsensitive)
                    || columns.value(5).toString().contains("PERSISTENT", Qt::CaseInsensitive),
                    "Expected a persistent generated uniqueness key.");
            auto expression = columns.value(6).toString().toLower();
            expression.remove(QRegularExpression("[`\\s()]"));
            auto reviewed = column["modifiers"].toString().toLower().section("case", 1).section("persistent", 0, 0);
            reviewed.prepend("case");
            reviewed.remove(QRegularExpression("[`\\s()]"));
            // SHOW CREATE can qualify literal character sets in the generated expression.
            expression.remove("_ascii");
            require(expression == reviewed, "Generated uniqueness expression differs.");
         }
         const auto type = column["type"].toString();
         if (type.startsWith("CHAR") || type.startsWith("VARCHAR") || type == "MEDIUMTEXT") {
            const bool ascii = column["modifiers"].toString().contains("CHARACTER SET ascii");
            require(columns.value(3).toString() == (ascii ? "ascii" : "utf8mb4") && columns.value(4).toString() == (ascii ? "ascii_bin" : "utf8mb4_unicode_ci"), "Schema text encoding differs from registered migration.");
         }
      }
      require(!columns.next(), "Unexpected schema column.");
      // MariaDB can hide TABLE_CONSTRAINTS CHECK rows from SELECT-only accounts.
      // SHOW CREATE remains available without granting runtime/migrator writes.
      QSqlQuery definition(database);
      require(definition.exec("SHOW CREATE TABLE `" + table + "`") && definition.next(),
              "Cannot inspect migration constraints with SHOW CREATE TABLE.");
      const auto ddl = definition.value(1).toString();
      auto namesOfConstraints = [&](const QString &kind) {
         return SchemaConstraints::namesFromDdl(ddl, kind);
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
      QJsonArray primary = descriptor["primaryKey"].toArray();
      if (primary.isEmpty()) for (const auto &column : expectedColumns) {
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
   QStringList fingerprints;
   while (history.next()) {
      ++current;
      require(current <= 3 && history.value(0).toInt() == current, "Unsupported/gapped/newer schema history; use a compatible TaskCompass build.");
      require(history.value(1).toByteArray() == QCryptographicHash::hash(migrationSql(current), QCryptographicHash::Sha256).toHex(), "Migration checksum mismatch; do not edit applied migrations.");
      require(history.value(3).toLongLong() > 0, "Invalid migration timestamp.");
      fingerprints.append(history.value(2).toString());
   }
   require(current > 0 && (requiredVersion == 0 || current == requiredVersion), "Required schema version is unavailable; request the reviewed migration, never auto-upgrade.");
   requireShape(current);
   require(fingerprints.back() == fingerprint(current), "Schema fingerprint changed; operator review required.");
   if (current == 3) { requireSchema3Modes(); }
   if (current >= 2) {
      QSqlQuery attempt(database);
      require(attempt.exec("SELECT version,checksum,baselineFingerprint,startedAt,completedAt,lastCompletedStep,attemptState FROM MigrationAttempt ORDER BY version"), "Cannot inspect migration attempts.");
      for (int version = 2; version <= current; ++version) {
         require(attempt.next() && attempt.value(0).toInt() == version
                 && attempt.value(1).toByteArray() == QCryptographicHash::hash(migrationSql(version), QCryptographicHash::Sha256).toHex()
                 && attempt.value(2).toString() == fingerprints[version - 2]
                 && attempt.value(3).toLongLong() > 0 && !attempt.value(4).isNull()
                 && attempt.value(4).toLongLong() >= attempt.value(3).toLongLong()
                 && attempt.value(5).toInt() == (version == 2 ? 9 : 10)
                 && attempt.value(6).toString() == "completed", "Interrupted/inconsistent migration attempt; explicit review required, no resume/repair.");
      }
      require(!attempt.next(), "Unexpected migration attempt; explicit review required.");
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
   const auto statements = migrationStatements(2);
   require(statements.size() == 9, "Migration 002 statement manifest changed; explicit review required.");
   const auto startedAt = QDateTime::currentMSecsSinceEpoch();
   bool attempted = false;
   bool committingMetadata = false;
   try {
      for (int step = 0; step < statements.size(); ++step) {
         QSqlQuery query(database);
         if (!query.exec(statements[step])) {
            bool validCode = false;
            const auto code = query.lastError().nativeErrorCode().toInt(&validCode);
            // Report only step/numeric code, never raw driver text or SQL data.
            throw std::runtime_error(QString("Migration 002 DDL step %1 failed (MariaDB code %2); preserve state for explicit review, no resume/reset/repair.")
               .arg(step + 1).arg(validCode ? QString::number(code) : QString("unavailable")).toStdString());
         }
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


void MigrationRunner::requireSchema3Modes() {
   QSqlQuery query(database);
   require(query.exec("SELECT @@SESSION.sql_mode,@@SESSION.foreign_key_checks,@@SESSION.check_constraint_checks")
           && query.next() && query.value(1).toInt() == 1 && query.value(2).toInt() == 1,
           "Schema 3 requires foreign-key and CHECK enforcement.");
   const auto modes = query.value(0).toString().split(',');
   require((modes.contains("STRICT_ALL_TABLES") || modes.contains("STRICT_TRANS_TABLES"))
           && modes.contains("NO_ZERO_DATE") && modes.contains("NO_ZERO_IN_DATE")
           && !modes.contains("ALLOW_INVALID_DATES"), "Schema 3 requires strict calendar-date SQL modes.");
}

QByteArray MigrationRunner::schema2DataDigest() {
   QFile file(":/migrations/schema.json");
   require(file.open(QIODevice::ReadOnly), "Missing schema 2 preservation projection.");
   const auto schema = QJsonDocument::fromJson(file.readAll()).object()["2"].toObject();
   QCryptographicHash hash(QCryptographicHash::Sha256);
   for (const auto &table : tableNamesForVersion(2)) {
      QStringList projection;
      for (const auto &column : schema[table].toObject()["columns"].toArray()) {
         projection.append("HEX(CAST(`" + column.toObject()["name"].toString() + "` AS BINARY))");
      }
      require(!projection.empty(), "Missing reviewed preservation projection.");
      const bool journal = table == "SchemaMigration" || table == "MigrationAttempt";
      QSqlQuery query(database);
      require(query.exec("SELECT " + projection.join(',') + " FROM `" + table + "`"
                         + (journal ? " WHERE version<=2 ORDER BY version" : " ORDER BY CAST(id AS BINARY)")),
              "Cannot inspect schema-2 preservation projection.");
      hash.addData(table.toUtf8());
      while (query.next()) {
         for (qsizetype i = 0; i < projection.size(); ++i) {
            const auto bytes = query.value(i).toByteArray();
            hash.addData(query.value(i).isNull() ? QByteArray("N;") : QByteArray::number(bytes.size()) + ':' + bytes + ';');
         }
      }
      require(!query.lastError().isValid(), "Schema-2 preservation query was interrupted.");
   }
   return hash.result();
}

void MigrationRunner::applyContexts(const std::string &expectedDataset, const QString &expectedFingerprint) {
   static const QRegularExpression SHA256("^[0-9a-f]{64}$");
   require(isUuid(expectedDataset) && SHA256.match(expectedFingerprint).hasMatch()
           && database.databaseName() == "taskcompass_phase13_test" && database.userName() == "taskcompass_phase13_test",
           "Schema 3 application requires the dedicated Phase 1.3 fixture with reviewed dataset and fingerprint.");
   requireIdentity(true);
   requireSchema3Modes();
   QSqlQuery lock(database);
   require(lock.exec("SELECT GET_LOCK(CONCAT('TaskCompassMigration:',DATABASE()),0)") && lock.next()
           && lock.value(0).toInt() == 1, "Another migration owns this schema.");
   struct Release {
      QSqlDatabase database;
      ~Release() { QSqlQuery query(database); query.exec("SELECT RELEASE_LOCK(CONCAT('TaskCompassMigration:',DATABASE()))"); }
   } release{database};
   MutationLease mutation(database, 0);
   require(verify(2) == expectedDataset && fingerprint(2) == expectedFingerprint,
           "Schema 2 identity/fingerprint differs from the reviewed fixture.");
   const auto originalData = schema2DataDigest();
   const auto statements = migrationStatements(3);
   require(statements.size() == 8, "Migration 003 statement manifest changed.");
   const auto checksum = QString::fromLatin1(QCryptographicHash::hash(migrationSql(3), QCryptographicHash::Sha256).toHex());
   const auto startedAt = QDateTime::currentMSecsSinceEpoch();
   const auto dataset = QString::fromStdString(expectedDataset);
   QSqlQuery ids(database);
   ids.prepare("SELECT id FROM Task WHERE datasetId=? ORDER BY CAST(id AS BINARY)");
   ids.addBindValue(dataset);
   require(ids.exec(), "Cannot list schema-2 task IDs before upgrade.");
   QStringList taskIds;
   while (ids.next()) { taskIds.append(ids.value(0).toString()); }
   require(!ids.lastError().isValid(), "Task ID discovery was interrupted.");
   ids.finish();
   QSqlQuery query(database);
   query.prepare("INSERT INTO MigrationAttempt (version,checksum,baselineFingerprint,startedAt,completedAt,lastCompletedStep,attemptState)"
                 " VALUES (3,?,?,?,NULL,1,'running')");
   query.addBindValue(checksum);
   query.addBindValue(expectedFingerprint);
   query.addBindValue(startedAt);
   require(query.exec() && query.numRowsAffected() == 1, "003 reservation failed or was not acknowledged; inspect before retrying.");
   const auto advance = [&](int prior, int next) {
      QSqlQuery step(database);
      step.prepare("UPDATE MigrationAttempt SET lastCompletedStep=? WHERE version=3 AND checksum=? AND attemptState='running' AND lastCompletedStep=?");
      step.addBindValue(next);
      step.addBindValue(checksum);
      step.addBindValue(prior);
      require(step.exec() && step.numRowsAffected() == 1, "003 step acknowledgment failed; inspect actual state.");
   };
   bool commitUncertain = false;
   try {
      if (afterStep) { afterStep(1); }
      int step = 1;
      for (qsizetype ddl = 0; ddl < statements.size(); ++ddl) {
         if (!query.exec(statements[ddl])) {
            bool numeric = false;
            const auto code = query.lastError().nativeErrorCode().toInt(&numeric);
            throw std::runtime_error(QString("003 DDL %1 failed (MariaDB code %2); preserve and inspect.")
               .arg(ddl + 1).arg(numeric ? QString::number(code) : QString("unavailable")).toStdString());
         }
         if (afterStep) { afterStep(-static_cast<int>(ddl + 1)); }
         advance(step, step + 1);
         if (afterStep) { afterStep(step + 1); }
         ++step;
         if (ddl != 2) { continue; }
         require(database.transaction(), "Cannot start rank backfill.");
         for (qsizetype row = 0; row < taskIds.size(); ++row) {
            query.prepare("UPDATE Task SET manualOrderKey=? WHERE datasetId=? AND id=? AND manualOrderKey IS NULL");
            query.addBindValue(QByteArray::fromStdString(seedRank(row + 1, taskIds.size())));
            query.addBindValue(dataset);
            query.addBindValue(taskIds[row]);
            require(query.exec() && query.numRowsAffected() == 1, "Backfill CAS failed; rollback and preserve attempt.");
         }
         query.prepare("SELECT id,HEX(manualOrderKey),priority FROM Task WHERE datasetId=? ORDER BY manualOrderKey,CAST(id AS BINARY)");
         query.addBindValue(dataset);
         require(query.exec(), "Cannot verify backfill.");
         for (qsizetype row = 0; row < taskIds.size(); ++row) {
            require(query.next() && query.value(0).toString() == taskIds[row]
                    && QByteArray::fromHex(query.value(1).toByteArray()).toStdString() == seedRank(row + 1, taskIds.size())
                    && query.value(2).toLongLong() == 5, "Rank backfill differs from reviewed seed order.");
         }
         require(!query.next() && !query.lastError().isValid(), "Unexpected backfill result.");
         query.finish();
         advance(step, step + 1);
         commitUncertain = true;
         require(database.commit(), "003 backfill commit outcome unknown; inspect before recovery.");
         commitUncertain = false;
         if (afterStep) { afterStep(step + 1); }
         ++step;
      }
      requireShape(3);
      require(schema2DataDigest() == originalData, "Schema 2 projections changed. Preserve and investigate.");
      const auto afterFingerprint = fingerprint(3);
      require(database.transaction(), "Cannot start 003 metadata transaction.");
      query.prepare("INSERT INTO SchemaMigration (version,checksum,schemaFingerprint,appliedAt) VALUES (3,?,?,?)");
      query.addBindValue(checksum);
      query.addBindValue(afterFingerprint);
      query.addBindValue(startedAt);
      require(query.exec() && query.numRowsAffected() == 1, "Cannot record schema 3.");
      query.prepare("UPDATE MigrationAttempt SET completedAt=?,attemptState='completed'"
                    " WHERE version=3 AND checksum=? AND attemptState='running' AND lastCompletedStep=10");
      query.addBindValue(std::max(startedAt, QDateTime::currentMSecsSinceEpoch()));
      query.addBindValue(checksum);
      require(query.exec() && query.numRowsAffected() == 1, "Cannot complete 003 journal.");
      if (afterStep) { afterStep(11); }
      commitUncertain = true;
      require(database.commit(), "003 metadata commit outcome unknown; inspect before recovery.");
      require(verify(3) == expectedDataset, "Schema 3 postconditions failed.");
   } catch (...) {
      database.rollback();
      if (!commitUncertain && database.isOpen()) {
         QSqlQuery failed(database);
         failed.prepare("UPDATE MigrationAttempt SET attemptState='failed' WHERE version=3 AND checksum=? AND attemptState='running'");
         failed.addBindValue(checksum);
         failed.exec();
      }
      throw;
   }
}

}
