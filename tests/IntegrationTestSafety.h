#pragma once

#include <QList>
#include <QStringList>
#include <stdexcept>

// Pure policy used by live tests and database-free safety tests. This class never
// connects or executes SQL. A fresh instance cannot authorize any reset.
class IntegrationTestSafety {
public:
   struct Table {
      QString name;
      QString type;
      QString engine;
   };
   struct Catalog {
      QList<Table> tables;
      QStringList routines;
      QStringList triggers;
      QStringList events;
   };

   static bool hasExplicitOptIn(const QStringList &arguments) {
      return arguments.size() == 3 && arguments.at(1) == "--run" && arguments.at(2) == "--allow-reset";
   }

   void begin(const Catalog &catalog, int lowerCaseTableNames) {
      if (ownsSchema || lowerCaseTableNames < 0 || lowerCaseTableNames > 2) {
         throw std::runtime_error("Invalid integration run state or table-name case setting.");
      }
      requireNoStoredCode(catalog);
      if (!catalog.tables.isEmpty()) {
         throw std::runtime_error("Integration database must start completely empty; existing tables/views are never reset at startup.");
      }
      caseSensitivity = lowerCaseTableNames == 0 ? Qt::CaseSensitive : Qt::CaseInsensitive;
      ownsSchema = true;
   }

   void requireResetAllowed(const Catalog &catalog) const {
      if (!ownsSchema) {
         throw std::runtime_error("Reset requires a fresh-schema check in this process; no startup/resume cleanup is permitted.");
      }
      requireNoStoredCode(catalog);
      const QStringList expected = {"LocalChange", "CommandRecord", "Task", "Dataset", "SchemaMigration"};
      for (const auto &table : catalog.tables) {
         if (!expected.contains(table.name, caseSensitivity) || table.type != "BASE TABLE"
             || table.engine.compare("InnoDB", Qt::CaseInsensitive) != 0) {
            throw std::runtime_error("Unexpected integration schema object/type/engine; refusing reset.");
         }
      }
   }

private:
   static void requireNoStoredCode(const Catalog &catalog) {
      if (!catalog.routines.isEmpty() || !catalog.triggers.isEmpty() || !catalog.events.isEmpty()) {
         throw std::runtime_error("Stored routines/triggers/events are unexpected in the integration schema; refusing changes.");
      }
   }
   bool ownsSchema = false;
   Qt::CaseSensitivity caseSensitivity = Qt::CaseSensitive;
};
