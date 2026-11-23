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
#include <QComboBox>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QPushButton>
#include <QMessageBox>
#include <QSignalSpy>
#include <QTableView>
#include <QTextEdit>
#include <QTreeView>
#include <QMenu>
#include <QToolButton>
#include <QMimeData>
#include <QIcon>
#include <QPalette>
#include <QToolBar>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QUuid>
#include <limits>

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
   void subtreeCountsIconsAndExclusions() {
      auto snapshot = sample();
      const auto root = organizationKey(snapshot.organizations[0]);
      const auto project = organizationKey(snapshot.organizations[1]);
      const auto objective = organizationKey(snapshot.organizations[2]);
      auto subpillar = std::get<Pillar>(snapshot.organizations[0]); subpillar.id = uuid(); subpillar.name = "Sub"; subpillar.parentPillarId = root.id;
      auto branch = std::get<Project>(snapshot.organizations[1]); branch.id = uuid(); branch.pillarId = subpillar.id;
      auto nested = branch; nested.id = uuid(); nested.pillarId.reset(); nested.parentProjectId = project.id;
      auto outcome = std::get<Objective>(snapshot.organizations[2]); outcome.id = uuid(); outcome.projectId.reset(); outcome.pillarId = subpillar.id;
      snapshot.organizations.insert(snapshot.organizations.end(), {subpillar, branch, nested, outcome});
      auto addTask = [&](TaskOwner owner) {
         auto task = snapshot.tasks.front(); task.id = uuid(); task.owner = owner; snapshot.tasks.push_back(task);
      };
      addTask({TaskOwnerKind::PROJECT, project.id}); addTask({TaskOwnerKind::PROJECT, nested.id});
      addTask({TaskOwnerKind::PROJECT, branch.id}); addTask({TaskOwnerKind::OBJECTIVE, outcome.id});
      addTask({});
      auto completed = snapshot.tasks.front(); completed.id = uuid(); completed.status = TaskStatus::COMPLETED; completed.completedAt = 1000;
      snapshot.tasks.push_back(completed);
      auto deleted = snapshot.tasks.front(); deleted.id = uuid(); deleted.deletedAt = 1000; snapshot.tasks.push_back(deleted);
      OrganizationTreeModel model;
      auto count = [&](OrganizationKey key) { return model.indexFor(key).data(OrganizationTreeModel::OPEN_COUNT_ROLE).toULongLong(); };
      model.replace(snapshot, true, false);
      QCOMPARE(count(root), 5ULL); QCOMPARE(count(project), 3ULL); QCOMPARE(count(objective), 1ULL);
      QCOMPARE(count(organizationKey(subpillar)), 2ULL); QCOMPARE(count(organizationKey(nested)), 1ULL);
      QVERIFY(model.index(2, 0).data().toString().contains("Pillars"));
      QVERIFY(!model.index(2, 0).data().toString().contains("Organization"));
      QCOMPARE(model.indexFor(root).data().toString(), QString("Home (5)"));
      for (const auto &index : model.organizationIndexes()) {
         QVERIFY(!index.data().toString().startsWith(index.data(OrganizationTreeModel::TYPE_ROLE).toString() + ":"));
         QVERIFY(index.data(Qt::ToolTipRole).toString().contains(index.data(OrganizationTreeModel::ID_ROLE).toString()));
         QVERIFY(index.data(Qt::AccessibleTextRole).toString().contains(index.data(OrganizationTreeModel::TYPE_ROLE).toString()));
         QVERIFY(index.data(Qt::AccessibleDescriptionRole).toString().contains(index.data(OrganizationTreeModel::PATH_ROLE).toString()));
      }
      const auto oldPalette = QApplication::palette();
      for (const auto foreground : {QColor(Qt::black), QColor(Qt::white)}) {
         QPalette palette = oldPalette; palette.setColor(QPalette::Text, foreground); QApplication::setPalette(palette);
         const auto pillarImage = qvariant_cast<QIcon>(model.indexFor(root).data(Qt::DecorationRole)).pixmap(48, 48).toImage();
         const auto projectImage = qvariant_cast<QIcon>(model.indexFor(project).data(Qt::DecorationRole)).pixmap(48, 48).toImage();
         const auto objectiveImage = qvariant_cast<QIcon>(model.indexFor(objective).data(Qt::DecorationRole)).pixmap(48, 48).toImage();
         QVERIFY(!pillarImage.isNull()); QVERIFY(pillarImage != projectImage); QVERIFY(projectImage != objectiveImage);
         bool visible = false;
         for (int y = 0; y < pillarImage.height(); ++y) {
            for (int x = 0; x < pillarImage.width(); ++x) {
               const auto pixel = pillarImage.pixelColor(x, y);
               if (pixel.alpha() > 128) { visible = true; QCOMPARE(pixel.red(), foreground.red()); }
            }
         }
         QVERIFY(visible);
      }
      QApplication::setPalette(oldPalette);
      std::get<Project>(snapshot.organizations[5]).archivedAt = 1000;
      model.replace(snapshot, true, false); QCOMPARE(count(root), 4ULL); QCOMPARE(count(project), 2ULL);
      std::get<Pillar>(snapshot.organizations[3]).archivedAt = 1000;
      model.replace(snapshot, true, false); QCOMPARE(count(root), 2ULL); QCOMPARE(count(organizationKey(branch)), 0ULL);
      std::get<Pillar>(snapshot.organizations[3]).archivedAt.reset();
      model.replace(snapshot, true, true); QCOMPARE(count(objective), 1ULL); QCOMPARE(count(organizationKey(outcome)), 1ULL);
      // Sorting is by the raw name, independent of count text and UUID order.
      auto alpha = std::get<Pillar>(snapshot.organizations[0]); alpha.id = uuid(); alpha.name = "alpha";
      snapshot.organizations.push_back(alpha);
      model.replace(snapshot, true, false);
      QCOMPARE(model.index(3, 0).data(OrganizationTreeModel::ID_ROLE).toString(), QString::fromStdString(alpha.id));
      QVERIFY(model.index(3, 0).data().toString().endsWith("(0)"));
   }
   void priorityEditorAcceptsFullSignedRange() {
      auto store = std::make_shared<MemoryWorkspaceRepository>();
      store->snapshot = sample();
      store->snapshot.schemaVersion = 3;
      PersistentTaskWindow window(factory(store)); window.show();
      auto *controller = window.findChild<PersistentTaskController *>();
      QTRY_VERIFY(controller->isReady() && !controller->isBusy());
      auto *table = window.findChild<QTableView *>("persistentTaskTable");
      auto *action = window.findChild<QAction *>("taskPriorityAction");
      table->selectRow(0);
      QTRY_VERIFY(action->isEnabled());
      bool constrained = false;
      QTimer::singleShot(0, [&]() {
         auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
         if (!dialog) { return; }
         auto *input = dialog->findChild<QLineEdit *>("taskPriorityInput");
         auto *buttons = dialog->findChild<QDialogButtonBox *>();
         if (!input || !buttons) { dialog->reject(); return; }
         auto *ok = buttons->button(QDialogButtonBox::Ok);
         const bool untouchedDisabled = !ok->isEnabled();
         input->setText("9223372036854775808");
         const bool overflowDisabled = !ok->isEnabled();
         input->setText("9223372036854775807");
         constrained = untouchedDisabled && overflowDisabled && ok->isEnabled();
         ok->click();
         if (!constrained) { dialog->reject(); }
      });
      action->trigger();
      QTRY_VERIFY(!controller->isBusy());
      QVERIFY(constrained);
      QCOMPARE(controller->workspace().tasks.front().priority, std::numeric_limits<std::int64_t>::max());
      controller->undo();
      QTRY_VERIFY(!controller->isBusy());
      QCOMPARE(controller->workspace().tasks.front().priority, 5);
   }

   void contextPickerUsesSelectedIdentityWhenCaptionsCollide() {
      auto store = std::make_shared<MemoryWorkspaceRepository>();
      store->snapshot = sample();
      store->snapshot.schemaVersion = 3;
      const auto first = uuid();
      const auto second = uuid();
      const auto literal = uuid();
      store->snapshot.contexts = {
         {first, DATASET, "Desk", "1", 1000, 1000, {}, {}, 1},
         {second, DATASET, "Desk", "11", 1000, 1000, {}, {}, 1},
         {literal, DATASET, "Desk (" + first + ")", "111", 1000, 1000, {}, {}, 1}
      };
      PersistentTaskWindow window(factory(store)); window.show();
      auto *controller = window.findChild<PersistentTaskController *>();
      QTRY_VERIFY(controller->isReady() && !controller->isBusy());
      auto *table = window.findChild<QTableView *>("persistentTaskTable");
      auto *action = window.findChild<QAction *>("assignContextAction");
      table->selectRow(0);
      QTRY_VERIFY(action->isEnabled());
      bool distinguishable = false;
      QTimer::singleShot(0, [&]() {
         auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
         if (!dialog) { return; }
         auto *picker = dialog->findChild<QComboBox *>("contextPicker");
         if (!picker || picker->count() != 4) { dialog->reject(); return; }
         distinguishable = picker->itemText(1) != picker->itemText(3);
         picker->setCurrentIndex(3);
         dialog->accept();
      });
      action->trigger();
      QTRY_VERIFY(!controller->isBusy());
      QVERIFY(distinguishable);
      const auto &snapshot = controller->workspace();
      const auto *link = primaryContext(snapshot, snapshot.tasks.front().id);
      QVERIFY(link);
      QCOMPARE(link->contextId, literal);
   }

   void countRefreshPreservesNavigationAndSharedHistory() {
      QSettings settings(QSettings::IniFormat, QSettings::UserScope, "TaskCompass", "Desktop"); settings.clear();
      auto store = std::make_shared<MemoryWorkspaceRepository>(); store->snapshot = sample();
      PersistentTaskWindow window(factory(store)); window.show();
      auto *controller = window.findChild<PersistentTaskController *>();
      QTRY_VERIFY(controller->isReady() && !controller->isBusy());
      auto *tree = window.findChild<QTreeView *>("organizationTree");
      auto *model = qobject_cast<OrganizationTreeModel *>(tree->model());
      const auto root = organizationKey(store->snapshot.organizations[0]);
      const auto objective = organizationKey(store->snapshot.organizations[2]);
      tree->setExpanded(model->indexFor(root), true); tree->setCurrentIndex(model->indexFor(objective));
      window.findChild<QLineEdit *>("searchEdit")->setText("no matching tasks");
      auto count = [&]() { return model->indexFor(root).data(OrganizationTreeModel::OPEN_COUNT_ROLE).toULongLong(); };
      QCOMPARE(count(), 1ULL);
      auto task = store->snapshot.tasks.front();
      controller->execute({TaskOperation::COMPLETE, task.id, task.localRevision, {}, {}});
      QTRY_VERIFY(!controller->isBusy()); QCOMPARE(count(), 0ULL);
      QCOMPARE(model->keyAt(tree->currentIndex()), std::optional(objective)); QVERIFY(tree->isExpanded(model->indexFor(root)));
      controller->undo(); QTRY_VERIFY(!controller->isBusy()); QCOMPARE(count(), 1ULL);
      controller->redo(); QTRY_VERIFY(!controller->isBusy()); QCOMPARE(count(), 0ULL);
      task = store->snapshot.tasks.front();
      controller->execute({TaskOperation::REOPEN, task.id, task.localRevision, {}, {}});
      QTRY_VERIFY(!controller->isBusy()); QCOMPARE(count(), 1ULL);
      TaskCommand create; create.title = "Another"; create.ownerChange = task.owner;
      controller->execute(create); QTRY_VERIFY(!controller->isBusy()); QCOMPARE(count(), 2ULL);
      auto created = store->snapshot.tasks.back();
      TaskCommand edit{TaskOperation::EDIT, created.id, created.localRevision, created.title, created.notes}; edit.ownerChange = TaskOwner{};
      controller->execute(edit); QTRY_VERIFY(!controller->isBusy()); QCOMPARE(count(), 1ULL);
      window.close(); settings.clear();
   }
   void usabilityModelTypesSortingAndDropIntent() {
      auto snapshot = sample();
      auto root = std::get<Pillar>(snapshot.organizations[0]);
      auto child = root; child.id = uuid(); child.name = "zebra"; child.parentPillarId = root.id;
      auto sibling = child; sibling.id = uuid(); sibling.name = "Alpha";
      auto other = root; other.id = uuid(); other.name = "Another";
      auto subproject = std::get<Project>(snapshot.organizations[1]);
      subproject.id = uuid(); subproject.pillarId.reset();
      subproject.parentProjectId = organizationFields(snapshot.organizations[1]).id;
      snapshot.organizations.insert(snapshot.organizations.end(), {child, sibling, other, subproject});
      OrganizationTreeModel model; model.replace(snapshot, true, false);
      QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
      const auto rootIndex = model.indexFor(organizationKey(root));
      QCOMPARE(rootIndex.data(OrganizationTreeModel::TYPE_ROLE).toString(), QString("Pillar"));
      QCOMPARE(model.indexFor(organizationKey(child)).data(OrganizationTreeModel::TYPE_ROLE).toString(), QString("Subpillar"));
      QCOMPARE(model.indexFor(organizationKey(subproject)).data(OrganizationTreeModel::TYPE_ROLE).toString(), QString("Subproject"));
      QCOMPARE(model.indexFor(organizationKey(snapshot.organizations[1])).data(OrganizationTreeModel::TYPE_ROLE).toString(), QString("Project"));
      QCOMPARE(model.indexFor(organizationKey(snapshot.organizations[2])).data(OrganizationTreeModel::TYPE_ROLE).toString(), QString("Objective"));
      QCOMPARE(model.index(0, 0, rootIndex).data(OrganizationTreeModel::ID_ROLE).toString(), QString::fromStdString(sibling.id));
      QVERIFY(!(model.flags(model.index(2, 0)) & Qt::ItemIsSelectable));
      QVERIFY(!(model.flags(rootIndex) & Qt::ItemIsDragEnabled));
      model.setReparentingEnabled(true);
      std::unique_ptr<QMimeData> drag(model.mimeData({rootIndex}));
      QVERIFY(!model.canDropMimeData(drag.get(), Qt::MoveAction, -1, 0, model.indexFor(organizationKey(child))));
      QVERIFY(!model.canDropMimeData(drag.get(), Qt::MoveAction, -1, 0, model.indexFor(organizationKey(snapshot.organizations[1]))));
      const auto destination = model.indexFor(organizationKey(other));
      QVERIFY(model.canDropMimeData(drag.get(), Qt::MoveAction, -1, 0, destination));
      QVERIFY(!model.canDropMimeData(drag.get(), Qt::CopyAction, -1, 0, destination));
      QVERIFY(!model.canDropMimeData(drag.get(), Qt::MoveAction, 0, 0, destination));
      QSignalSpy intent(&model, &OrganizationTreeModel::reparentRequested);
      QVERIFY(model.dropMimeData(drag.get(), Qt::MoveAction, -1, 0, destination));
      QCOMPARE(intent.count(), 1);
      QCOMPARE(model.parent(rootIndex), QModelIndex()); // Drop never mutates the projection.
      model.replace(snapshot, true, false);
      QVERIFY(!model.canDropMimeData(drag.get(), Qt::MoveAction, -1, 0, model.indexFor(organizationKey(other))));
   }
   void droppedMoveUsesPreviewCommandAndUndo() {
      QSettings settings(QSettings::IniFormat, QSettings::UserScope, "TaskCompass", "Desktop"); settings.clear();
      auto store = std::make_shared<MemoryWorkspaceRepository>(); store->snapshot = sample();
      PersistentTaskWindow window(factory(store)); window.show();
      auto *controller = window.findChild<PersistentTaskController *>();
      QTRY_VERIFY(controller->isReady() && !controller->isBusy());
      auto *tree = window.findChild<QTreeView *>("organizationTree");
      auto *model = qobject_cast<OrganizationTreeModel *>(tree->model());
      const auto objective = organizationKey(store->snapshot.organizations[2]);
      const auto pillar = organizationKey(store->snapshot.organizations[0]);
      const auto original = store->snapshot;
      // First cancel: neither projection nor repository changes.
      QTimer cancel;
      connect(&cancel, &QTimer::timeout, this, []() {
         if (auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget())) { dialog->reject(); }
      });
      cancel.start(10);
      std::unique_ptr<QMimeData> drag(model->mimeData({model->indexFor(objective)}));
      QVERIFY(model->dropMimeData(drag.get(), Qt::MoveAction, -1, 0, model->indexFor(pillar)));
      QTest::qWait(50); cancel.stop(); QCOMPARE(store->snapshot, original);
      // Then accept the existing parent picker and subtree preview.
      int previews = 0;
      QTimer accept;
      connect(&accept, &QTimer::timeout, this, [&]() {
         if (auto *message = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
            QVERIFY(message->text().contains("Subtree:")); ++previews; message->button(QMessageBox::Yes)->click();
         } else if (auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget())) { dialog->accept(); }
      });
      accept.start(10);
      QVERIFY(model->dropMimeData(drag.get(), Qt::MoveAction, -1, 0, model->indexFor(pillar)));
      QTRY_VERIFY(organizationParent(store->snapshot.organizations[2]) == pillar);
      QTRY_VERIFY(!controller->isBusy()); accept.stop(); QCOMPARE(previews, 1);
      QCOMPARE(store->snapshot.tasks, original.tasks);
      QCOMPARE(model->indexFor(organizationKey(original.organizations[1])).data(OrganizationTreeModel::OPEN_COUNT_ROLE).toULongLong(), 0ULL);
      controller->undo(); QTRY_VERIFY(!controller->isBusy());
      QCOMPARE(organizationParent(store->snapshot.organizations[2]), organizationParent(original.organizations[2]));
      controller->redo(); QTRY_VERIFY(!controller->isBusy());
      QVERIFY(organizationParent(store->snapshot.organizations[2]) == pillar);
      window.close(); settings.clear();
   }
   void creationMenusAndArchivePreferences() {
      QSettings settings(QSettings::IniFormat, QSettings::UserScope, "TaskCompass", "Desktop");
      settings.clear(); settings.setValue("showArchived", true);
      auto store = std::make_shared<MemoryWorkspaceRepository>(); store->snapshot = sample();
      auto archived = std::get<Pillar>(store->snapshot.organizations[0]); archived.id = uuid(); archived.name = "Archived"; archived.archivedAt = 1000;
      store->snapshot.organizations.push_back(archived);
      PersistentTaskWindow window(factory(store)); window.show();
      auto *controller = window.findChild<PersistentTaskController *>();
      QTRY_VERIFY(controller->isReady() && !controller->isBusy());
      auto *tree = window.findChild<QTreeView *>("organizationTree");
      auto *model = qobject_cast<OrganizationTreeModel *>(tree->model());
      QVERIFY(!window.findChild<QCheckBox *>("showArchived")->isChecked());
      QVERIFY(!model->indexFor(organizationKey(archived)).isValid());
      auto *toolbar = window.findChild<QToolBar *>("persistentToolbar");
      QCOMPARE(toolbar->widgetForAction(toolbar->actions().front()), window.findChild<QToolButton *>("createButton"));
      QVERIFY(!toolbar->actions().contains(window.findChild<QAction *>("newTaskAction")));
      QVERIFY(!toolbar->actions().contains(window.findChild<QAction *>("newPillarAction")));
      auto *menu = window.findChild<QMenu *>("creationMenu");
      auto labels = [&]() {
         QMetaObject::invokeMethod(menu, "aboutToShow");
         QStringList result;
         for (auto *action : menu->actions()) { result << (action->isSeparator() ? "|" : action->text().remove('&')); }
         return result;
      };
      tree->setCurrentIndex(model->index(0, 0));
      QCOMPARE(labels(), QStringList({"New Task...", "|", "New Pillar..."}));
      tree->setCurrentIndex(model->indexFor(organizationKey(store->snapshot.organizations[0])));
      QCOMPARE(labels(), QStringList({"New Task...", "|", "New Subpillar...", "New Project...", "New Objective...", "|", "New Pillar..."}));
      tree->expandAll();
      const auto point = tree->visualRect(tree->currentIndex()).center();
      QTimer::singleShot(0, []() {
         auto *context = qobject_cast<QMenu *>(QApplication::activePopupWidget()); QVERIFY(context);
         QStringList actual;
         for (auto *action : context->actions().mid(0, 7)) { actual << (action->isSeparator() ? "|" : action->text().remove('&')); }
         QCOMPARE(actual, QStringList({"New Task...", "|", "New Subpillar...", "New Project...", "New Objective...", "|", "New Pillar..."}));
         context->close();
      });
      QMetaObject::invokeMethod(tree, "customContextMenuRequested", Q_ARG(QPoint, point));
      tree->setCurrentIndex(model->indexFor(organizationKey(store->snapshot.organizations[1])));
      QCOMPARE(labels(), QStringList({"New Task...", "|", "New Subproject...", "New Objective...", "|", "New Pillar..."}));
      tree->setCurrentIndex(model->indexFor(organizationKey(store->snapshot.organizations[2])));
      QCOMPARE(labels(), QStringList({"New Task...", "|", "New Pillar..."}));
      for (const int selected : {0, 1, 2}) {
         tree->setCurrentIndex(model->indexFor(organizationKey(store->snapshot.organizations[selected])));
         QTimer::singleShot(0, [&]() {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget()); QVERIFY(dialog);
            const auto owner = dialog->findChild<QLineEdit *>("taskOwner")->text();
            if (selected == 0) { QCOMPARE(owner, QString("Independent")); }
            else { QVERIFY(owner.contains(selected == 1 ? "Project" : "Outcome")); }
            dialog->reject();
         });
         window.findChild<QAction *>("newTaskAction")->trigger();
      }
      const auto selected = organizationKey(store->snapshot.organizations[1]);
      tree->setCurrentIndex(model->indexFor(selected));
      tree->setExpanded(model->indexFor(organizationKey(store->snapshot.organizations[0])), true);
      window.findChild<QCheckBox *>("showArchived")->setChecked(true);
      window.findChild<QCheckBox *>("showArchived")->setChecked(false);
      controller->reload(); QTRY_VERIFY(!controller->isBusy());
      QCOMPARE(model->keyAt(tree->currentIndex()), std::optional(selected));
      QVERIFY(tree->isExpanded(model->indexFor(organizationKey(store->snapshot.organizations[0]))));
      window.findChild<QCheckBox *>("showArchived")->setChecked(true); window.close();
      PersistentTaskWindow reopened(factory(store));
      QVERIFY(!reopened.findChild<QCheckBox *>("showArchived")->isChecked());
      QTRY_VERIFY(reopened.findChild<PersistentTaskController *>()->isReady() && !reopened.findChild<PersistentTaskController *>()->isBusy());
      auto *reopenedTree = reopened.findChild<QTreeView *>("organizationTree");
      QVERIFY(!static_cast<OrganizationTreeModel *>(reopenedTree->model())->indexFor(organizationKey(archived)).isValid());
      reopened.close(); settings.clear();
   }
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
         QCOMPARE(model.rowCount(), 4);
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
         QCOMPARE(model->indexFor(root).data(OrganizationTreeModel::OPEN_COUNT_ROLE).toULongLong(), 0ULL);
         tree->setCurrentIndex(model->indexFor(objective));
         QVERIFY(!window.findChild<QAction *>("restoreOrganizationAction")->isEnabled());
         window.findChild<QAction *>("locateArchivedAncestorAction")->trigger();
         QCOMPARE(tree->currentIndex().data(OrganizationTreeModel::ID_ROLE).toString(), QString::fromStdString(root.id));
         confirmMessage(); window.findChild<QAction *>("restoreOrganizationAction")->trigger();
         QTRY_VERIFY(!controller->isBusy());
         QVERIFY(!organizationFields(store->snapshot.organizations[0]).archivedAt);
         QCOMPARE(model->indexFor(root).data(OrganizationTreeModel::OPEN_COUNT_ROLE).toULongLong(), 1ULL);
         // All new-task entry points default to a valid selected owner.
         tree->setCurrentIndex(model->indexFor(objective));
         QTimer::singleShot(0, []() {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog); dialog->findChild<QLineEdit *>("titleEdit")->setText("Independent capture"); dialog->accept();
         });
         window.findChild<QAction *>("newTaskAction")->trigger(); QTRY_VERIFY(!controller->isBusy());
         QCOMPARE(store->snapshot.tasks.back().owner.kind, TaskOwnerKind::OBJECTIVE);
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
