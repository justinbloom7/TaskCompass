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
   parser.addOption({"apply", "Apply migration 001 to the explicitly configured isolated database (operator approval required)."});
   parser.addOption({"verify", "Read-only schema/version verification."});
   parser.addOption({"integration", "Use TASKCOMPASS_TEST_DB_* and taskcompass_integration, not development configuration."});
   parser.process(application);
   try {
      if (parser.isSet("apply") && parser.isSet("verify")) {
         throw std::runtime_error("Choose --apply or --verify, not both.");
      }
      if (!parser.isSet("apply") && !parser.isSet("verify")) {
         QTextStream(stdout) << taskcompass::MigrationRunner::migrationSql();
         return 0;
      }
      const auto config = taskcompass::DatabaseConfig::fromEnvironment(parser.isSet("integration"));
      if (parser.isSet("apply") && config.user != (parser.isSet("integration") ? "taskcompass_test" : "taskcompass_migrate")) {
         throw std::runtime_error("Migration --apply requires the dedicated migration/test account, never runtime/admin credentials.");
      }
      taskcompass::DatabaseConnection connection(config);
      taskcompass::MigrationRunner runner(connection.database());
      if (parser.isSet("apply")) {
         runner.applyInitial();
      }
      QTextStream(stdout) << "Schema 1 verified; dataset " << QString::fromStdString(runner.verify()) << '\n';
      return 0;
   } catch (const std::exception &error) {
      QTextStream(stderr) << error.what() << '\n';
      return 1;
   }
}
