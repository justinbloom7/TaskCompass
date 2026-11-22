#include "DatabaseConnection.h"
#include "MigrationRunner.h"

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
   parser.addOption({"to-version", "Explicit migration/verification target, 1 or 2 (apply defaults to 1).", "version"});
   parser.addOption({"print-migration", "Print one embedded migration without connecting.", "version"});
   parser.process(application);
   try {
      if (parser.isSet("integration") && parser.isSet("phase12-test")) { throw std::runtime_error("Choose one explicit test target."); }
      bool validTarget = true;
      const int target = parser.isSet("to-version") ? parser.value("to-version").toInt(&validTarget) : (parser.isSet("apply") ? 1 : 0);
      if (!validTarget || (parser.isSet("to-version") && target == 0) || (target != 0 && target != 1 && target != 2)) { throw std::runtime_error("Unsupported migration target."); }
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
      const auto config = parser.isSet("phase12-test") ? taskcompass::DatabaseConfig::fromPhase12Environment()
                                                     : taskcompass::DatabaseConfig::fromEnvironment(parser.isSet("integration"));
      if (parser.isSet("apply") && config.user != (parser.isSet("phase12-test") ? "taskcompass_phase12_test"
                                                   : parser.isSet("integration") ? "taskcompass_test" : "taskcompass_migrate")) {
         throw std::runtime_error("Migration --apply requires the dedicated migration/test account, never runtime/admin credentials.");
      }
      taskcompass::DatabaseConnection connection(config);
      taskcompass::MigrationRunner runner(connection.database());
      if (parser.isSet("apply")) {
         runner.applyToVersion(target);
      }
      const auto datasetId = runner.verify(target);
      QTextStream(stdout) << "Schema " << runner.verifiedVersion() << " verified; dataset " << QString::fromStdString(datasetId) << '\n';
      return 0;
   } catch (const std::exception &error) {
      QTextStream(stderr) << error.what() << '\n';
      return 1;
   }
}
