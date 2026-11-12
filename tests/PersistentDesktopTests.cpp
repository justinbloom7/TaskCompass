#include "MemoryTaskRepository.h"
#include "desktop/persistent/PersistentTaskWindow.h"
#include "desktop/persistent/PersistentTaskModel.h"

#include <QAbstractItemModelTester>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QUuid>

namespace {
std::string uuid() { return QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString(); }
PersistentTaskController::SessionFactory factory() {
   return []() {
      auto session = std::make_unique<PersistentTaskSession>();
      session->repository = std::make_unique<MemoryTaskRepository>();
      session->service = std::make_unique<taskcompass::TaskService>(*session->repository, uuid(),
                                                                    []() { return 1000; }, uuid);
      session->service->execute({taskcompass::TaskOperation::CREATE, {}, 0, "Alpha", {}});
      session->service->execute({taskcompass::TaskOperation::CREATE, {}, 0, "Bravo", {}});
      session->service->clearHistory();
      return session;
   };
}
}

class PersistentDesktopTests : public QObject {
   Q_OBJECT
private slots:
   void modelAndCommitGate() {
      PersistentTaskModel model;
      QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
      taskcompass::Task task{uuid(), uuid(), "Task", {}, taskcompass::TaskStatus::ACTIVE, 1000, 1000, {}, {}, 1};
      model.replaceTasks({task});
      QSignalSpy requests(&model, &PersistentTaskModel::completionRequested);
      const auto index = model.index(0, PersistentTaskModel::STATUS_COLUMN);
      QVERIFY(!model.setData(index, Qt::Checked, Qt::CheckStateRole));
      model.setWritable(true);
      QVERIFY(model.setData(index, Qt::Checked, Qt::CheckStateRole));
      QCOMPARE(requests.count(), 1);
      QCOMPARE(model.data(index, Qt::CheckStateRole).toInt(), static_cast<int>(Qt::Unchecked));
      QVERIFY(!model.setData(index, Qt::PartiallyChecked, Qt::CheckStateRole));
      task.status = taskcompass::TaskStatus::COMPLETED;
      task.completedAt = 1000;
      model.replaceTasks({task});
      QCOMPARE(model.data(index, Qt::CheckStateRole).toInt(), static_cast<int>(Qt::Checked));
   }

   void controllerFailureAndShutdown() {
      PersistentTaskController controller(factory());
      QSignalSpy failures(&controller, &PersistentTaskController::failed);
      controller.reload();
      QTRY_VERIFY(controller.isReady() && !controller.isBusy());
      const auto original = controller.tasks();
      controller.execute({taskcompass::TaskOperation::CREATE, {}, 0, " ", {}});
      QTRY_COMPARE(failures.count(), 1);
      QVERIFY(!controller.isReady());
      QVERIFY(controller.tasks() == original);
      controller.reload();
      QTRY_VERIFY(controller.isReady());
      QVERIFY(!controller.canUndo());
   }

   void uncertainCommitRequiresReloadAndClearsHistory() {
      class UncertainRepository : public MemoryTaskRepository {
      public:
         void commit(const taskcompass::CommandReceipt &receipt) override {
            MemoryTaskRepository::commit(receipt);
            if (receipt.after.title == "Unknown outcome") {
               throw taskcompass::CommitOutcomeUnknown("Injected lost commit acknowledgment");
            }
         }
      };
      PersistentTaskController controller([]() {
         auto session = std::make_unique<PersistentTaskSession>();
         session->repository = std::make_unique<UncertainRepository>();
         session->service = std::make_unique<taskcompass::TaskService>(*session->repository, uuid(),
                                                                       []() { return 1000; }, uuid);
         return session;
      });
      QSignalSpy failures(&controller, &PersistentTaskController::failed);
      controller.reload();
      QTRY_VERIFY(controller.isReady() && !controller.isBusy());
      controller.execute({taskcompass::TaskOperation::CREATE, {}, 0, "Confirmed", {}});
      QTRY_VERIFY(!controller.isBusy());
      QVERIFY(controller.canUndo());
      controller.execute({taskcompass::TaskOperation::CREATE, {}, 0, "Unknown outcome", {}});
      QTRY_COMPARE(failures.count(), 1);
      QVERIFY(!controller.isReady() && !controller.canUndo());
      controller.reload();
      QTRY_VERIFY(controller.isReady() && !controller.isBusy());
      QCOMPARE(controller.tasks().size(), std::size_t(2));
      QVERIFY(!controller.canUndo());
   }

   void taskActionsFilteringAndSelection() {
      PersistentTaskWindow window(factory());
      window.show();
      auto *controller = window.findChild<PersistentTaskController *>();
      auto *table = window.findChild<QTableView *>("persistentTaskTable");
      auto *showCompleted = window.findChild<QCheckBox *>("showCompleted");
      auto *proxy = qobject_cast<QSortFilterProxyModel *>(table->model());
      QAbstractItemModelTester tester(proxy, QAbstractItemModelTester::FailureReportingMode::QtTest);
      QTRY_VERIFY(controller->isReady() && !controller->isBusy());
      QCOMPARE(proxy->rowCount(), 2);
      table->selectRow(0);
      const auto selectedId = proxy->index(0, 0).data(Qt::UserRole).toString();
      auto *edit = window.findChild<QAction *>("editTaskAction");
      QTimer::singleShot(0, []() {
         auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
         QVERIFY(dialog);
         dialog->findChild<QLineEdit *>("titleEdit")->setText("Zulu edited");
         dialog->accept();
      });
      edit->trigger();
      QTRY_VERIFY(!controller->isBusy());
      QCOMPARE(table->currentIndex().data(Qt::UserRole).toString(), selectedId);
      QCOMPARE(table->currentIndex().data().toString(), QString("Zulu edited"));
      auto *complete = window.findChild<QAction *>("completeTaskAction");
      complete->trigger();
      QTRY_VERIFY(!controller->isBusy());
      QCOMPARE(proxy->rowCount(), 1);
      showCompleted->setChecked(true);
      QCOMPARE(proxy->rowCount(), 2);
      for (int row = 0; row < proxy->rowCount(); ++row) {
         if (proxy->index(row, 0).data(Qt::UserRole).toString() == selectedId) {
            table->selectRow(row);
         }
      }
      QCOMPARE(complete->text(), QString("Reopen Task"));
      complete->trigger();
      QTRY_VERIFY(!controller->isBusy());
      showCompleted->setChecked(false);
      QCOMPARE(proxy->rowCount(), 2);
      auto *undo = window.findChild<QAction *>("undoAction");
      auto *redo = window.findChild<QAction *>("redoAction");
      undo->trigger();
      QTRY_VERIFY(!controller->isBusy());
      QCOMPARE(proxy->rowCount(), 1);
      redo->trigger();
      QTRY_VERIFY(!controller->isBusy());
      QCOMPARE(proxy->rowCount(), 2);
      const auto oldCount = controller->tasks().size();
      QTimer::singleShot(0, []() {
         auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
         QVERIFY(dialog);
         QVERIFY(!dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->isEnabled());
         dialog->reject();
      });
      window.findChild<QAction *>("newTaskAction")->trigger();
      QCOMPARE(controller->tasks().size(), oldCount);
      QTimer::singleShot(0, []() {
         auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
         QVERIFY(dialog);
         dialog->findChild<QLineEdit *>("titleEdit")->setText("New task");
         dialog->accept();
      });
      window.findChild<QAction *>("newTaskAction")->trigger();
      QTRY_VERIFY(!controller->isBusy());
      QCOMPARE(controller->tasks().size(), oldCount + 1);
      auto *search = window.findChild<QLineEdit *>("searchEdit");
      search->setText("New task");
      QCOMPARE(proxy->rowCount(), 1);
      window.close();
   }
};

int main(int argc, char *argv[]) {
   QApplication application(argc, argv);
   QTemporaryDir settings;
   if (!settings.isValid()) {
      return 1;
   }
   QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
   PersistentDesktopTests tests;
   return QTest::qExec(&tests, argc, argv);
}
#include "PersistentDesktopTests.moc"
