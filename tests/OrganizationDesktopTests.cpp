#include "MemoryWorkspaceRepository.h"
#include "desktop/models/OrganizationTreeModel.h"
#include "desktop/dialogs/OrganizationMoveDialog.h"
#include "desktop/dialogs/OrganizationEditDialog.h"
#include "desktop/persistent/PersistentTaskWindow.h"
#include "desktop/persistent/PersistentTaskModel.h"
#include "desktop/persistent/PersistentTaskFilter.h"
#include <QAbstractItemModelTester>
#include <QApplication>
#include <QAction>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QPushButton>
#include <QMessageBox>
#include <QSignalSpy>
#include <QTableView>
#include <QTextEdit>
#include <QTreeView>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QUuid>

using namespace taskcompass;
namespace {
std::string uuid() { return QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString(); }
const std::string DATASET = "00000000-0000-4000-8000-000000000001";
WorkspaceSnapshot sample() {
   Pillar pillar; pillar.id = uuid(); pillar.datasetId = DATASET; pillar.name = "Home";
   pillar.createdAt = pillar.updatedAt = 1000; pillar.localRevision = 1;
   Project project; static_cast<OrganizationFields &>(project) = pillar; project.id = uuid(); project.title = "Project"; project.pillarId = pillar.id;
   Objective objective; static_cast<OrganizationFields &>(objective) = pillar; objective.id = uuid(); objective.title = "Outcome"; objective.projectId = project.id;
   Task task{uuid(), DATASET, "Task", {}, TaskStatus::ACTIVE, 1000, 1000, {}, {}, 1, {TaskOwnerKind::OBJECTIVE, objective.id}};
   return {{task}, {pillar, project, objective}};
}
class SharedMemory : public WorkspaceRepository {
public:
   explicit SharedMemory(std::shared_ptr<MemoryWorkspaceRepository> store) : store(std::move(store)) {}
   WorkspaceSnapshot read() override { return store->read(); }
   WorkspaceReceipt transact(const Prepare &prepare) override { return store->transact(prepare); }
private:
   std::shared_ptr<MemoryWorkspaceRepository> store;
};
PersistentTaskController::SessionFactory factory(const std::shared_ptr<MemoryWorkspaceRepository> &store) {
   return [store]() {
      auto session = std::make_unique<PersistentTaskSession>();
      session->workspaceRepository = std::make_unique<SharedMemory>(store);
      session->commands = std::make_unique<CommandService>(*session->workspaceRepository, DATASET, []() { return 2000; }, uuid);
      return session;
   };
}
void confirmMessage() {
   QTimer::singleShot(0, []() {
      auto *message = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
      QVERIFY(message);
      QVERIFY(message->text().contains("2 descendant organizations and 1 tasks"));
      message->button(QMessageBox::Yes)->click();
   });
}
}

class OrganizationDesktopTests : public QObject {
   Q_OBJECT
private slots:
   void modelPathsFiltersAndDeepHierarchy() {
      auto snapshot = sample();
      OrganizationTreeModel model;
      {
         QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
         model.replace(snapshot, false, false);
         const auto objective = organizationKey(snapshot.organizations[2]);
         QVERIFY(model.indexFor(objective).isValid());
         QCOMPARE(model.pathFor(objective), QString("Home / Project / Outcome"));
         model.replace(snapshot, false, true);
         QCOMPARE(model.parent(model.indexFor(objective)), QModelIndex());
         QCOMPARE(model.rowCount(), 3);
      }
      PersistentTaskModel tasks;
      PersistentTaskFilter filter; filter.setSourceModel(&tasks);
      tasks.replaceWorkspace(snapshot); QCOMPARE(filter.rowCount(), 1);
      const auto oldTask = snapshot.tasks[0];
      std::get<Pillar>(snapshot.organizations[0]).archivedAt = 1000;
      tasks.replaceWorkspace(snapshot); QCOMPARE(filter.rowCount(), 0);
      filter.setIncludeArchived(true); QCOMPARE(filter.rowCount(), 1);
      QVERIFY(tasks.data(tasks.index(0, PersistentTaskModel::ORGANIZATION_COLUMN)).toString().contains("archived"));
      QCOMPARE(snapshot.tasks[0], oldTask);
      std::get<Pillar>(snapshot.organizations[0]).name = "Renamed";
      tasks.replaceWorkspace(snapshot);
      QVERIFY(tasks.data(tasks.index(0, PersistentTaskModel::ORGANIZATION_COLUMN)).toString().startsWith("Renamed"));
      // Flat storage and iterative index/parent walks, including destruction.
      snapshot = sample();
      auto parent = organizationFields(snapshot.organizations[0]).id;
      for (int depth = 0; depth < 2000; ++depth) {
         Pillar pillar; pillar.id = uuid(); pillar.datasetId = DATASET; pillar.name = "Depth";
         pillar.createdAt = pillar.updatedAt = 1000; pillar.localRevision = 1; pillar.parentPillarId = parent;
         parent = pillar.id; snapshot.organizations.emplace_back(pillar);
      }
      model.replace(snapshot, false, false);
      auto index = model.indexFor({OrganizationKind::PILLAR, parent});
      int depth = 0;
      while (index.isValid()) { ++depth; index = model.parent(index); }
      QCOMPARE(depth, 2001);
   }
   void duplicateNamesRemainDistinct() {
      auto snapshot = sample();
      auto duplicate = std::get<Objective>(snapshot.organizations[2]); duplicate.id = uuid();
      snapshot.organizations.emplace_back(duplicate);
      OrganizationTreeModel model; model.replace(snapshot, false, true);
      const auto first = organizationKey(snapshot.organizations[2]), second = organizationKey(snapshot.organizations[3]);
      QVERIFY(model.pathFor(first) != model.pathFor(second));
      QVERIFY(model.pathFor(second).contains(QString::fromStdString(second.id)));
      OrganizationMoveDialog picker(snapshot, {}, second, {}, false);
      QVERIFY(picker.parentChoice() == second);
   }
   void duplicateNamesAreDisambiguated() {
      auto snapshot = sample();
      auto duplicate = std::get<Objective>(snapshot.organizations.back()); duplicate.id = uuid();
      snapshot.organizations.emplace_back(duplicate);
      OrganizationTreeModel model; model.replace(snapshot, true, true);
      const auto first = organizationKey(snapshot.organizations[2]), second = organizationKey(snapshot.organizations[3]);
      QVERIFY(model.pathFor(first) != model.pathFor(second));
      QVERIFY(model.pathFor(first).contains(QString::fromStdString(first.id)));
      QVERIFY(model.indexFor(second).data().toString().contains("objective"));
   }
   void descriptionEditPreservesNullAndEmpty() {
      auto snapshot = sample();
      const auto original = snapshot.organizations[0];
      OrganizationEditDialog nullNotes(snapshot, OrganizationKind::PILLAR, original, {});
      QVERIFY(!nullNotes.command().description);
      organizationFields(snapshot.organizations[0]).description = "";
      OrganizationEditDialog emptyNotes(snapshot, OrganizationKind::PILLAR, snapshot.organizations[0], {});
      QVERIFY(emptyNotes.command().description.has_value());
      QCOMPARE(*emptyNotes.command().description, std::string());
      emptyNotes.findChild<QTextEdit *>()->setPlainText("Notes");
      QCOMPARE(*emptyNotes.command().description, std::string("Notes"));
   }
   void legalParentPicker() {
      const auto snapshot = sample();
      const auto root = organizationKey(snapshot.organizations[0]);
      const auto project = organizationKey(snapshot.organizations[1]);
      OrganizationMoveDialog move(snapshot, OrganizationKind::PILLAR, {}, root, true);
      auto *tree = move.findChild<QTreeView *>();
      auto *proxy = qobject_cast<QSortFilterProxyModel *>(tree->model());
      auto *model = qobject_cast<OrganizationTreeModel *>(proxy->sourceModel());
      QVERIFY(!proxy->mapFromSource(model->indexFor(root)).isValid());
      QVERIFY(!proxy->mapFromSource(model->indexFor(project)).isValid());
      OrganizationMoveDialog task(snapshot, {}, {}, {}, false);
      tree = task.findChild<QTreeView *>(); proxy = qobject_cast<QSortFilterProxyModel *>(tree->model());
      model = qobject_cast<OrganizationTreeModel *>(proxy->sourceModel());
      QVERIFY(!(proxy->flags(proxy->mapFromSource(model->indexFor(root))) & Qt::ItemIsSelectable));
      QVERIFY(proxy->flags(proxy->mapFromSource(model->indexFor(project))) & Qt::ItemIsSelectable);
   }
   void actionsArchiveRestoreAndRestart() {
      auto store = std::make_shared<MemoryWorkspaceRepository>(); store->snapshot = sample();
      const auto root = organizationKey(store->snapshot.organizations[0]);
      const auto objective = organizationKey(store->snapshot.organizations[2]);
      {
         PersistentTaskWindow window(factory(store)); window.show();
         auto *controller = window.findChild<PersistentTaskController *>();
         auto *tree = window.findChild<QTreeView *>("organizationTree");
         auto *model = qobject_cast<OrganizationTreeModel *>(tree->model());
         QTRY_VERIFY(controller->isReady() && !controller->isBusy());
         tree->setCurrentIndex(model->indexFor(root));
         auto *archive = window.findChild<QAction *>("archiveOrganizationAction");
         QVERIFY(archive->isEnabled()); confirmMessage(); archive->trigger();
         QTRY_VERIFY(!controller->isBusy());
         QVERIFY(!model->indexFor(root).isValid());
         auto *showArchived = window.findChild<QCheckBox *>("showArchived"); showArchived->setChecked(true);
         QVERIFY(model->indexFor(objective).isValid());
         tree->setCurrentIndex(model->indexFor(objective));
         QVERIFY(!window.findChild<QAction *>("restoreOrganizationAction")->isEnabled());
         window.findChild<QAction *>("locateArchivedAncestorAction")->trigger();
         QCOMPARE(tree->currentIndex().data(OrganizationTreeModel::ID_ROLE).toString(), QString::fromStdString(root.id));
         confirmMessage(); window.findChild<QAction *>("restoreOrganizationAction")->trigger();
         QTRY_VERIFY(!controller->isBusy());
         QVERIFY(!organizationFields(store->snapshot.organizations[0]).archivedAt);
         // Global capture stays independent even under a selected objective.
         tree->setCurrentIndex(model->indexFor(objective));
         QTimer::singleShot(0, []() {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog); dialog->findChild<QLineEdit *>("titleEdit")->setText("Independent capture"); dialog->accept();
         });
         window.findChild<QAction *>("newTaskAction")->trigger(); QTRY_VERIFY(!controller->isBusy());
         QCOMPARE(store->snapshot.tasks.back().owner.kind, TaskOwnerKind::INDEPENDENT);
         QVERIFY(controller->canUndo());
         tree->setCurrentIndex(model->indexFor(objective)); window.close();
      }
      PersistentTaskWindow restarted(factory(store)); restarted.show();
      auto *controller = restarted.findChild<PersistentTaskController *>();
      QTRY_VERIFY(controller->isReady() && !controller->isBusy());
      QCOMPARE(controller->tasks().size(), std::size_t(2)); QVERIFY(!controller->canUndo());
      auto *tree = restarted.findChild<QTreeView *>("organizationTree");
      QCOMPARE(tree->currentIndex().data(OrganizationTreeModel::ID_ROLE).toString(), QString::fromStdString(objective.id));
      restarted.close();
   }
};

int main(int argc, char **argv) {
   QApplication application(argc, argv);
   QTemporaryDir settings; if (!settings.isValid()) { return 1; }
   QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
   OrganizationDesktopTests tests; return QTest::qExec(&tests, argc, argv);
}
#include "OrganizationDesktopTests.moc"
