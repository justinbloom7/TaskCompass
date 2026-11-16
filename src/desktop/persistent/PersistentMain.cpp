#include "PersistentTaskWindow.h"
#include "infrastructure/storage/DatabaseConnection.h"
#include "infrastructure/storage/MigrationRunner.h"
#include "infrastructure/storage/MariaDbWorkspaceRepository.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDateTime>
#include <QUuid>

int main(int argc, char *argv[]) {
   QApplication application(argc, argv);
   QCoreApplication::setOrganizationName("TaskCompass");
   QCoreApplication::setApplicationName("TaskCompass");
   QCommandLineParser parser;
   parser.setApplicationDescription("Persistent TaskCompass desktop. Requires an explicitly migrated isolated database; never runs migrations.");
   parser.addHelpOption();
   parser.process(application);
   PersistentTaskWindow window([]() {
      auto session = std::make_unique<PersistentTaskSession>();
      const auto config = taskcompass::DatabaseConfig::fromEnvironment();
      if (config.user != "taskcompass_app") {
         throw std::runtime_error("The desktop requires the taskcompass_app runtime account; diagnostic/migration/admin accounts are not accepted.");
      }
      auto connection = std::make_shared<taskcompass::DatabaseConnection>(config);
      const auto datasetId = taskcompass::MigrationRunner(connection->database()).verify(2);
      session->connectionLifetime = connection;
      session->workspaceRepository = std::make_unique<taskcompass::MariaDbWorkspaceRepository>(connection->database(), datasetId);
      session->commands = std::make_unique<taskcompass::CommandService>(*session->workspaceRepository, datasetId,
                                                                    []() { return QDateTime::currentMSecsSinceEpoch(); },
                                                                    []() { return QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString(); });
      return session;
   });
   window.show();
   return application.exec();
}
