#include "MainWindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QTemporaryDir>
#include <QTimer>

int main(int argc, char *argv[]) {
   QApplication application(argc, argv);
   QCoreApplication::setOrganizationName("TaskCompass");
   QCoreApplication::setApplicationName("TaskCompassPrototype");
   QCoreApplication::setApplicationVersion("0.1.0");
   QCommandLineParser parser;
   parser.setApplicationDescription("Phase 0.1 desktop prototype; sample tasks are never persisted.");
   parser.addHelpOption();
   parser.addVersionOption();
   QCommandLineOption smokeOption("smoke-test", "Show the window briefly, then exit; use temporary layout settings.");
   parser.addOption(smokeOption);
   parser.process(application);

   QTemporaryDir smokeSettings;
   if (parser.isSet(smokeOption)) {
      if (!smokeSettings.isValid()) {
         return 1;
      }
      QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, smokeSettings.path());
   }
   MainWindow window;
   window.show();
   if (parser.isSet(smokeOption)) {
      QTimer::singleShot(500, &window, &QWidget::close);
   }
   return application.exec();
}
