#include "MigrationRunner.h"
#include "domain/Task.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QSqlQuery>
#include <QStringList>
#include <QUuid>
#include <QVariant>
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

MigrationRunner::MigrationRunner(QSqlDatabase database) : database(database) {}

QByteArray MigrationRunner::migrationSql() {
   initializeMigrations();
   QFile file(":/migrations/001.sql");
   require(file.open(QIODevice::ReadOnly), "Embedded migration SQL is unavailable.");
   return file.readAll();
}

QString MigrationRunner::fingerprint() {
   QByteArray schema;
   for (const auto &table : TABLES) {
      QSqlQuery query(database);
      require(query.exec("SHOW CREATE TABLE `" + table + "`") && query.next(),
              "Missing/inaccessible migration table. Verify schema; do not rerun partial DDL.");
      auto ddl = query.value(1).toString();
      // Windows lower_case_table_names can normalize table identifiers.
      for (const auto &name : TABLES) {
         ddl.replace("`" + name + "`", "`" + name.toLower() + "`", Qt::CaseInsensitive);
      }
      schema += ddl.toUtf8() + '\n';
   }
   return QString::fromLatin1(QCryptographicHash::hash(schema, QCryptographicHash::Sha256).toHex());
}

std::string MigrationRunner::verify() {
   auto expected = TABLES;
   for (auto &name : expected) {
      name = name.toLower();
   }
   expected.sort();
   require(tableNames(database) == expected,
           "Uninitialized, partial, or unexpected schema. Use the reviewed migration/recovery procedure.");
   QSqlQuery version(database);
   require(version.exec("SELECT version, checksum, schemaFingerprint FROM SchemaMigration ORDER BY version") && version.next(),
           "No completed migration recorded. Partial initialization requires operator recovery.");
   require(version.value(0).toInt() == 1, "Unsupported schema version; use a compatible TaskCompass build.");
   const auto checksum = QCryptographicHash::hash(migrationSql(), QCryptographicHash::Sha256).toHex();
   require(version.value(1).toByteArray() == checksum, "Migration checksum mismatch; do not edit applied migrations.");
   require(version.value(2).toString() == fingerprint(), "Schema fingerprint changed; operator review required.");
   require(!version.next(), "Newer or inconsistent schema history; upgrade the application.");
   QSqlQuery dataset(database);
   require(dataset.exec("SELECT id FROM Dataset") && dataset.next(), "Dataset is missing.");
   const auto id = dataset.value(0).toString().toStdString();
   require(isUuid(id) && !dataset.next(), "Exactly one canonical dataset is required in Phase 1.1.");
   return id;
}

void MigrationRunner::applyInitial() {
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
      verify(); // Idempotent only for a fully verified schema; never auto-resume partial DDL.
      return;
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
   verify();
}

}
