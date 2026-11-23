#include "DatabaseConnection.h"
#include "MigrationRunner.h"
#include "domain/Task.h"
#include <QRegularExpression>

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QTextStream>
#include <exception>

int main(int argc, char *argv[]) {
   QCoreApplication application(argc, argv);
   QCommandLineParser parser;
   parser.setApplicationDescription("Explicit TaskCompass schema tool. Default prints SQL only; never provisions accounts.");
   parser.addHelpOption();
   parser.addOption({"apply", "Apply the explicit target to the configured isolated database (operator approval required)."});
   parser.addOption({"verify", "Read-only schema/version verification."});
   parser.addOption({"integration", "Use TASKCOMPASS_TEST_DB_* and taskcompass_integration, not development configuration."});
   parser.addOption({"phase12-test", "Use only the separately approved taskcompass_phase12_test target."});
   parser.addOption({"phase13-test", "Use only the separately approved taskcompass_phase13_test target."});
   parser.addOption({"expected-dataset", "Reviewed schema-2 dataset UUID for migration 003.", "uuid"});
   parser.addOption({"expected-baseline-fingerprint", "Reviewed schema-2 fingerprint for migration 003.", "sha256"});
   parser.addOption({"to-version", "Explicit migration/verification target, 1, 2 or 3 (apply defaults to 1).", "version"});
   parser.addOption({"print-migration", "Print one embedded migration without connecting.", "version"});
   parser.process(application);
   try {
      const bool phase13 = parser.isSet("phase13-test");
      if (parser.isSet("integration") + parser.isSet("phase12-test") + phase13 > 1) { throw std::runtime_error("Choose one explicit test target."); }
      bool validTarget = true;
      const int target = parser.isSet("to-version") ? parser.value("to-version").toInt(&validTarget) : (parser.isSet("apply") ? 1 : 0);
      if (!validTarget || (parser.isSet("to-version") && target == 0) || (target < 0 || target > 3)) { throw std::runtime_error("Unsupported migration target."); }
      if (parser.isSet("print-migration")) {
         if (parser.isSet("apply") || parser.isSet("verify")) { throw std::runtime_error("Preview cannot execute or connect."); }
         const int printed = parser.value("print-migration").toInt(&validTarget);
         if (!validTarget) { throw std::runtime_error("Unknown migration version."); }
         QTextStream(stdout) << taskcompass::MigrationRunner::migrationSql(printed);
         return 0;
      }
      if (parser.isSet("apply") && parser.isSet("verify")) {
         throw std::runtime_error("Choose --apply or --verify, not both.");
      }
      if (!parser.isSet("apply") && !parser.isSet("verify")) {
         if (parser.isSet("to-version")) { throw std::runtime_error("Use --apply or --verify with --to-version; use --print-migration for preview."); }
         QTextStream(stdout) << taskcompass::MigrationRunner::migrationSql();
         return 0;
      }
      const auto expectedDataset = parser.value("expected-dataset").toStdString();
      const auto expectedFingerprint = parser.value("expected-baseline-fingerprint");
      if (parser.isSet("apply") && target == 3
          && (!phase13 || !taskcompass::isUuid(expectedDataset)
              || !QRegularExpression("^[0-9a-f]{64}$").match(expectedFingerprint).hasMatch())) {
         throw std::runtime_error("003 requires --phase13-test and the approved dataset and baseline fingerprint.");
      }
      const auto config = phase13 ? taskcompass::DatabaseConfig::fromPhase13Environment()
                                  : parser.isSet("phase12-test") ? taskcompass::DatabaseConfig::fromPhase12Environment()
                                  : taskcompass::DatabaseConfig::fromEnvironment(parser.isSet("integration"));
      if (parser.isSet("apply") && config.user != (phase13 ? "taskcompass_phase13_test"
                                                   : parser.isSet("phase12-test") ? "taskcompass_phase12_test"
                                                   : parser.isSet("integration") ? "taskcompass_test" : "taskcompass_migrate")) {
         throw std::runtime_error("Migration --apply requires the dedicated migration/test account, never runtime/admin credentials.");
      }
      const bool applyContexts = parser.isSet("apply") && target == 3;
      if (applyContexts) {
         taskcompass::DatabaseConnection migrationConnection(config);
         taskcompass::MigrationRunner(migrationConnection.database()).applyContexts(expectedDataset, expectedFingerprint);
      }
      taskcompass::DatabaseConnection connection(config);
      taskcompass::MigrationRunner runner(connection.database());
      if (parser.isSet("apply") && !applyContexts) { runner.applyToVersion(target); }
      const auto datasetId = runner.verify(target);
      QTextStream(stdout) << "Schema " << runner.verifiedVersion() << " verified; dataset " << QString::fromStdString(datasetId) << '\n';
      return 0;
   } catch (const std::exception &error) {
      QTextStream(stderr) << error.what() << '\n';
      return 1;
   }
}
