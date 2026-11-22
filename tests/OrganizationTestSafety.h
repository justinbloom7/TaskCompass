#pragma once

#include "IntegrationTestSafety.h"

// Separate allowlist/purpose: never expands the accepted Phase 1.1 reset policy.
class OrganizationTestSafety {
public:
   static bool hasOptIn(const QStringList &arguments) {
      return arguments == QStringList{arguments.value(0), "--run", "--allow-reset"};
   }
   void begin(const IntegrationTestSafety::Catalog &catalog, int lowerCaseTableNames) {
      if (owned || lowerCaseTableNames < 0 || lowerCaseTableNames > 2 || !catalog.tables.isEmpty()) {
         throw std::runtime_error("Phase 1.2 tests require completely empty fresh schema; never reset pre-existing objects.");
      }
      checkStoredCode(catalog);
      sensitivity = lowerCaseTableNames == 0 ? Qt::CaseSensitive : Qt::CaseInsensitive;
      owned = true;
   }
   void requireCleanup(const IntegrationTestSafety::Catalog &catalog) const {
      if (!owned) { throw std::runtime_error("No current-process ownership proof for Phase 1.2 cleanup."); }
      checkStoredCode(catalog);
      for (const auto &table : catalog.tables) {
         if (!tables().contains(table.name, sensitivity) || table.type != "BASE TABLE" || table.engine.compare("InnoDB", Qt::CaseInsensitive) != 0) {
            throw std::runtime_error("Unexpected Phase 1.2 schema contents; preserve evidence, no cleanup.");
         }
      }
   }
   static QStringList tables() { return {"LocalChange", "CommandRecord", "Task", "Objective", "Project", "Pillar", "MigrationAttempt", "Dataset", "SchemaMigration"}; }
private:
   static void checkStoredCode(const IntegrationTestSafety::Catalog &catalog) {
      if (!catalog.routines.isEmpty() || !catalog.triggers.isEmpty() || !catalog.events.isEmpty()) {
         throw std::runtime_error("Unexpected stored code in Phase 1.2 schema.");
      }
   }
   bool owned = false;
   Qt::CaseSensitivity sensitivity = Qt::CaseSensitive;
};
