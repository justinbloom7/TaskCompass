#include "infrastructure/storage/WorkspaceCodec.h"
#include "infrastructure/storage/MigrationRunner.h"
#include "infrastructure/storage/DatabaseConfig.h"
#include "infrastructure/storage/MutationLease.h"
#include "OrganizationTestSafety.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QCryptographicHash>
#include <QTextStream>
#include <QProcess>
#include <QProcessEnvironment>
#include <stdexcept>

using namespace taskcompass;
namespace {
void check(bool value) { if (!value) { throw std::runtime_error("Workspace codec/resource assertion failed"); } }
template<typename Action> void rejects(Action action) {
   bool failed = false;
   try { action(); } catch (const std::exception &) { failed = true; }
   check(failed);
}
}
int main(int argc, char **argv) {
   QCoreApplication application(argc, argv);
   try {
      const std::string id = "00000000-0000-4000-8000-000000000001";
      const std::string dataset = "00000000-0000-4000-8000-000000000002";
      Task task{id, dataset, "Unicode 🧭", "", TaskStatus::ACTIVE, 1000, 9007199254740993LL, {}, {}, 1};
      auto json = encodeEntity(task);
      check(std::get<Task>(decodeEntity(json)) == task);
      json.remove("entityType"); json.remove("objectiveId"); json.remove("directProjectId");
      check(std::get<Task>(decodeEntity(json, 1)) == task);
      rejects([&]() { decodeEntity(json, 3); });
      json["entityType"] = "task"; json["objectiveId"] = QString::fromStdString(id); json["directProjectId"] = QString::fromStdString(id);
      rejects([&]() { decodeEntity(json); });
      json = encodeEntity(task); json["updatedAt"] = 9007199254740992.0;
      rejects([&]() { decodeEntity(json); });
      Pillar pillar; pillar.id = id; pillar.datasetId = dataset; pillar.name = "Area";
      pillar.createdAt = 1000; pillar.updatedAt = 2000; pillar.archivedAt = 1500; pillar.localRevision = 1;
      Project project; static_cast<OrganizationFields &>(project) = pillar; project.title = "Project"; project.pillarId = id;
      project.id = "00000000-0000-4000-8000-000000000003";
      Objective objective; static_cast<OrganizationFields &>(objective) = pillar; objective.title = "Objective"; objective.pillarId = id;
      for (const auto &entity : std::vector<EntitySnapshot>{pillar, project, objective}) { check(decodeEntity(encodeEntity(entity)) == entity); }
      const auto sql1 = MigrationRunner::migrationSql(1);
      check(QCryptographicHash::hash(sql1, QCryptographicHash::Sha256).toHex() == "bb901d7580398893b5c04ee263f813b9d05873eb4bae08e0daf377faf73b0ea0");
      check(MigrationRunner::tableNamesForVersion(1).size() == 5 && MigrationRunner::tableNamesForVersion(2).size() == 9);
      check(application.arguments().size() == 4);
      QFile source(application.arguments()[1]); check(source.open(QIODevice::ReadOnly));
      const auto sql2 = source.readAll(); check(sql2 == MigrationRunner::migrationSql(2));
      QTextStream(stdout) << "Reviewed DDL statement counts: " << MigrationRunner::migrationStatements(1).size()
                          << " / " << MigrationRunner::migrationStatements(2).size() << '\n';
      check(MigrationRunner::migrationStatements(2).size() == 9);
      check(MigrationRunner::migrationStatements(1).size() == 5);
      for (const auto &statement : MigrationRunner::migrationStatements(2)) { check(!statement.trimmed().isEmpty()); }
      check(sql2.count(';') == 9 && sql2.contains("commandTargetV2") && sql2.contains("taskOwnerChoice"));
      rejects([&]() { MigrationRunner::migrationSql(4); });
      rejects([&]() { MigrationRunner::tableNamesForVersion(0); });
      DatabaseConfig test{"127.0.0.1", 3306, "taskcompass_phase12_test", "taskcompass_phase12_test", "not-a-secret"};
      test.validate(); test.validateIdentity("taskcompass_phase12_test", "taskcompass_phase12_test@127.0.0.1");
      auto wrong = test; wrong.databaseName = "taskcompass_integration";
      rejects([&]() { wrong.validate(); });
      wrong = test; wrong.user = "taskcompass_test"; rejects([&]() { wrong.validate(); });
      IntegrationTestSafety::Catalog emptyCatalog;
      IntegrationTestSafety::Catalog ownedCatalog;
      ownedCatalog.tables.append({"Objective", "BASE TABLE", "InnoDB"});
      OrganizationTestSafety safety;
      rejects([&]() { safety.requireCleanup(emptyCatalog); });
      rejects([&]() { safety.begin(ownedCatalog, 0); });
      safety.begin(emptyCatalog, 0);
      safety.requireCleanup(ownedCatalog);
      rejects([&]() { safety.begin(emptyCatalog, 0); });
      auto unexpected = ownedCatalog; unexpected.tables[0].name = "Other";
      rejects([&]() { safety.requireCleanup(unexpected); });
      unexpected = ownedCatalog; unexpected.tables[0].engine = "MyISAM";
      rejects([&]() { safety.requireCleanup(unexpected); });
      unexpected = ownedCatalog; unexpected.triggers.append("Unreviewed");
      rejects([&]() { safety.requireCleanup(unexpected); });
      OrganizationTestSafety windows; windows.begin(emptyCatalog, 1);
      ownedCatalog.tables[0].name = "objective"; windows.requireCleanup(ownedCatalog);
      rejects([&]() { safety.requireCleanup(ownedCatalog); });
      check(OrganizationTestSafety::hasOptIn({"suite", "--run", "--allow-reset"}));
      for (const auto &arguments : QList<QStringList>{{"suite"}, {"suite", "--run"}, {"suite", "--allow-reset", "--run"},
                                                      {"suite", "--run", "--allow-reset", "extra"}}) {
         check(!OrganizationTestSafety::hasOptIn(arguments));
      }
      rejects([&]() { MutationLease invalid{QSqlDatabase()}; });
      rejects([&]() { MigrationRunner(QSqlDatabase()).applyToVersion(2); });
      auto environment = QProcessEnvironment::systemEnvironment();
      for (const auto &key : environment.keys()) { if (key.startsWith("TASKCOMPASS_")) { environment.remove(key); } }
      auto runGuard = [&](const QString &program, const QStringList &arguments, int code, const QByteArray &message) {
         QProcess process; process.setProcessEnvironment(environment); process.start(program, arguments);
         check(process.waitForFinished(10000) && process.exitStatus() == QProcess::NormalExit && process.exitCode() == code);
         const auto output = process.readAllStandardOutput() + process.readAllStandardError();
         check(output.contains(message) && !output.contains("not-a-secret"));
      };
      const auto migrate = application.arguments()[2], integration = application.arguments()[3];
      for (const auto &target : {"0", "4", "garbage", "-1"}) {
         runGuard(migrate, {"--verify", "--to-version", target}, 1, "Unsupported migration target");
      }
      runGuard(migrate, {"--to-version", "2"}, 1, "Use --apply or --verify");
      runGuard(migrate, {"--print-migration", "2"}, 0, "CREATE TABLE Pillar");
      runGuard(migrate, {"--apply", "--phase12-test"}, 1, "Explicit isolated Phase 1.2");
      environment.insert("TASKCOMPASS_DB_HOST", "127.0.0.1"); environment.insert("TASKCOMPASS_DB_NAME", "taskcompass_dev");
      environment.insert("TASKCOMPASS_DB_USER", "taskcompass_app"); environment.insert("TASKCOMPASS_DB_ISOLATED", "1");
      environment.insert("TASKCOMPASS_DB_PASSWORD", "not-a-secret");
      runGuard(migrate, {"--apply", "--to-version", "2"}, 1, "dedicated migration/test account");
      runGuard(integration, {}, 77, "SKIPPED");
      environment.insert("TASKCOMPASS_PHASE12_TEST_DB_HOST", "127.0.0.1"); environment.insert("TASKCOMPASS_PHASE12_TEST_DB_ISOLATED", "1");
      environment.insert("TASKCOMPASS_PHASE12_TEST_DB_PASSWORD", "not-a-secret");
      environment.insert("TASKCOMPASS_PHASE12_TEST_DB_USER", "taskcompass_phase12_test");
      for (const auto &name : {"taskcompass_dev", "taskcompass_integration", "other"}) {
         environment.insert("TASKCOMPASS_PHASE12_TEST_DB_NAME", name);
         runGuard(integration, {"--run", "--allow-reset"}, 1, "Explicit isolated Phase 1.2");
      }
      QTextStream(stdout) << "PASS: four typed snapshot codecs, immutable v1 interpretation, exact 64-bit values, migration resources and isolated configuration guards; no live SQL.\n";
      return 0;
   } catch (const std::exception &error) { QTextStream(stderr) << error.what() << '\n'; return 1; }
}
