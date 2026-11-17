// SPDX-License-Identifier: GPL-3.0-or-later
#include "infrastructure/storage/SchemaConstraints.h"
#include "infrastructure/storage/MigrationRunner.h"

#include <QCoreApplication>
#include <QRegularExpression>
#include <QTextStream>
#include <stdexcept>

using namespace taskcompass;
namespace {
void check(bool value) {
   if (!value) { throw std::runtime_error("SHOW CREATE constraint assertion failed"); }
}
template<typename Action> void rejects(Action action) {
   bool failed = false;
   try { action(); } catch (const std::exception &) { failed = true; }
   check(failed);
}
}

int main(int argc, char **argv) {
   QCoreApplication application(argc, argv);
   try {
      // MariaDB's SHOW CREATE formatting, including a composite FK. No database
      // or TABLE_CONSTRAINTS catalog is consulted by this inspection path.
      const QString ddl = "CREATE TABLE `CommandRecord` (\n"
         "  `id` char(36) NOT NULL,\n"
         "  PRIMARY KEY (`id`),\n"
         "  CONSTRAINT `commandTask` FOREIGN KEY (`datasetId`, `taskId`) REFERENCES `Task` (`datasetId`, `id`),\n"
         "  CONSTRAINT `commandPayload` CHECK (`payloadVersion` = 1 and json_valid(`afterSnapshot`)),\n"
         "  CONSTRAINT `commandKind` CHECK (`commandType` in ('create','edit'))\n"
         ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4";
      const QStringList expectedChecks{"commandKind", "commandPayload"};
      check(SchemaConstraints::namesFromDdl(ddl, "CHECK") == expectedChecks);
      check(SchemaConstraints::namesFromDdl(ddl, "FOREIGN KEY") == QStringList{"commandTask"});
      auto windows = ddl;
      windows.replace("\n", "\r\n");
      check(SchemaConstraints::namesFromDdl(windows, "CHECK") == expectedChecks);
      auto changed = ddl;
      changed.replace("commandPayload", "unexpectedCheck");
      check(SchemaConstraints::namesFromDdl(changed, "CHECK") != expectedChecks);
      changed = ddl;
      changed.remove("  CONSTRAINT `commandKind` CHECK (`commandType` in ('create','edit'))\n");
      check(SchemaConstraints::namesFromDdl(changed, "CHECK") != expectedChecks);
      changed = ddl;
      changed.replace("  PRIMARY KEY", "  CONSTRAINT `extra` CHECK (1),\n  PRIMARY KEY");
      check(SchemaConstraints::namesFromDdl(changed, "CHECK") != expectedChecks);
      changed = ddl;
      changed.replace("commandKind", "commandPayload");
      rejects([&]() { SchemaConstraints::namesFromDdl(changed, "CHECK"); });
      changed = ddl;
      changed.replace("CONSTRAINT `commandKind` CHECK", "CONSTRAINT `commandKind` UNKNOWN");
      rejects([&]() { SchemaConstraints::namesFromDdl(changed, "CHECK"); });
      rejects([&]() { SchemaConstraints::namesFromDdl("", "CHECK"); });
      rejects([&]() { SchemaConstraints::namesFromDdl(ddl, "UNKNOWN"); });
      check(SchemaConstraints::namesFromDdl("CREATE TABLE `Empty` (\n  `value` int\n) ENGINE=InnoDB", "CHECK").isEmpty());
      changed = ddl;
      changed.replace("commandKind", "escaped``name");
      check(SchemaConstraints::namesFromDdl(changed, "CHECK").contains("escaped`name"));
      // Exercise every real CREATE TABLE in both embedded migrations. Quote the
      // source identifiers to emulate SHOW CREATE, without executing any SQL.
      int checks = 0, foreignKeys = 0, tables = 0;
      for (int version : {1, 2}) {
         for (auto statement : MigrationRunner::migrationStatements(version)) {
            if (!statement.startsWith("CREATE TABLE ")) { continue; }
            ++tables;
            // Source SQL wraps expressions; SHOW CREATE renders each constraint
            // on one line. Join only the source's indented continuation lines.
            statement.replace(QRegularExpression("\\n {6,}"), " ");
            statement.replace(QRegularExpression("CONSTRAINT ([A-Za-z][A-Za-z0-9]*) "), "CONSTRAINT `\\1` ");
            checks += SchemaConstraints::namesFromDdl(statement, "CHECK").size();
            foreignKeys += SchemaConstraints::namesFromDdl(statement, "FOREIGN KEY").size();
         }
      }
      check(tables == 9 && checks == 35 && foreignKeys == 12);
      QTextStream(stdout) << "PASS: SHOW CREATE constraints, missing/extra/renamed/duplicate rejection, CRLF, composite FKs and migration CREATE definitions; no SQL connection.\n";
      return 0;
   } catch (const std::exception &error) {
      QTextStream(stderr) << error.what() << '\n';
      return 1;
   }
}
