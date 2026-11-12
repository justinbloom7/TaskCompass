#include <QCommandLineParser>
#include <QCoreApplication>
#include <QProcessEnvironment>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextStream>
#include <QVariant>

int main(int argc, char *argv[]) {
   QCoreApplication application(argc, argv);
   QCoreApplication::setApplicationName("TaskCompassDbDiagnostic");
   QCommandLineParser parser;
   parser.setApplicationDescription("Optional read-only QMYSQL connectivity probe; no task data is accessed.");
   parser.addHelpOption();
   QCommandLineOption connectOption("connect", "Connect using explicit TASKCOMPASS_DB_* environment variables.");
   parser.addOption(connectOption);
   parser.process(application);

   QTextStream output(stdout);
   QTextStream error(stderr);
   const QStringList drivers = QSqlDatabase::drivers();
   const bool hasMysql = drivers.contains("QMYSQL");
   output << "Qt SQL drivers: " << (drivers.isEmpty() ? "(none)" : drivers.join(", ")) << '\n';
   output << "QMYSQL available: " << (hasMysql ? "yes" : "no") << '\n';
   bool canLoadMysql = false;
   if (hasMysql) {
      {
         const QSqlDatabase probe = QSqlDatabase::addDatabase("QMYSQL", "DriverLoadProbe");
         canLoadMysql = probe.isValid();
      }
      QSqlDatabase::removeDatabase("DriverLoadProbe");
   }
   output << "QMYSQL loadable: " << (canLoadMysql ? "yes" : "no") << '\n';
   if (!canLoadMysql) {
      output << "Missing Qt 6 QMYSQL plugin (CentOS: qt6-qtbase-mysql), or its matching connector library.\n";
   }
   if (!parser.isSet(connectOption)) {
      output << "Connection skipped: no connection requested. Use --connect and an isolated database.\n";
      return 0;
   }

   const QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
   const QString host = environment.value("TASKCOMPASS_DB_HOST");
   const QString databaseName = environment.value("TASKCOMPASS_DB_NAME");
   const QString user = environment.value("TASKCOMPASS_DB_USER");
   const QString password = environment.value("TASKCOMPASS_DB_PASSWORD");
   if (host.trimmed().isEmpty() || databaseName.trimmed().isEmpty() || user.trimmed().isEmpty()
       || environment.value("TASKCOMPASS_DB_ISOLATED") != "1") {
      error << "Configuration error: set TASKCOMPASS_DB_HOST, TASKCOMPASS_DB_NAME, TASKCOMPASS_DB_USER,\n"
               "and TASKCOMPASS_DB_ISOLATED=1 for a dedicated test database, never eEnvelopes.\n";
      return 3;
   }
   bool validPort = false;
   const int port = environment.value("TASKCOMPASS_DB_PORT", "3306").toInt(&validPort);
   if (!validPort || port < 1 || port > 65535) {
      error << "Configuration error: TASKCOMPASS_DB_PORT must be 1..65535.\n";
      return 3;
   }
   if (!canLoadMysql) {
      error << "Cannot connect: QMYSQL is unavailable. See docs/Building.md.\n";
      return 2;
   }

   auto safeError = [&password](QString message) {
      if (!password.isEmpty()) {
         message.replace(password, "<redacted>");
      }
      return message;
   };
   constexpr auto CONNECTION_NAME = "TaskCompassReadOnlyProbe";
   int result = 0;
   {
      QSqlDatabase database = QSqlDatabase::addDatabase("QMYSQL", CONNECTION_NAME);
      database.setHostName(host);
      database.setPort(port);
      database.setDatabaseName(databaseName);
      database.setUserName(user);
      database.setPassword(password);
      database.setConnectOptions("MYSQL_OPT_CONNECT_TIMEOUT=5;MYSQL_OPT_READ_TIMEOUT=5;MYSQL_OPT_WRITE_TIMEOUT=5");
      if (!database.open()) {
         error << "Connection failed: " << safeError(database.lastError().text()) << '\n';
         result = 4;
      } else {
         QSqlQuery query(database);
         if (!query.exec("SELECT VERSION()") || !query.next()) {
            error << "Read-only probe failed: " << safeError(query.lastError().text()) << '\n';
            result = 5;
         } else {
            output << "Connection succeeded. Server version: " << query.value(0).toString() << '\n';
            output << "Executed SELECT VERSION() only; no application records accessed.\n";
         }
      }
      database.close();
   }
   QSqlDatabase::removeDatabase(CONNECTION_NAME);
   return result;
}
