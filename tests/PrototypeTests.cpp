#include "desktop/MainWindow.h"
#include "desktop/dialogs/TaskEditDialog.h"
#include "desktop/models/TaskFilterModel.h"
#include "desktop/models/TaskTableModel.h"

#include <QAbstractItemModelTester>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QProcess>
#include <QProcessEnvironment>
#include <utility>
#include <QPushButton>
#include <QSettings>
#include <QSplitter>
#include <QTableView>
#include <QTemporaryDir>
#include <QTest>
#include <QTextEdit>
#include <QTimer>
#include <QTreeView>

class PrototypeTests : public QObject {
   Q_OBJECT

public:
   explicit PrototypeTests(QString executable) : diagnosticExecutable(std::move(executable)) {}

private slots:
   void init() {
      QSettings settings(QSettings::IniFormat, QSettings::UserScope, "TaskCompass", "DesktopPrototype");
      settings.clear();
   }

   void representativeModel() {
      TaskTableModel model;
      QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
      QCOMPARE(model.rowCount(), 6);
      QCOMPARE(model.columnCount(), 5);
      QCOMPARE(model.rowCount(model.index(0, 0)), 0);
      QVERIFY(!model.data(QModelIndex()).isValid());
      QVERIFY(!model.updateTask(-1, {}));
      QCOMPARE(model.appendTask({}), -1);
      SampleTask added;
      added.title = "New fixture";
      added.description = "Description";
      const int newRow = model.appendTask(added);
      QCOMPARE(newRow, 6);
      QCOMPARE(model.taskAt(newRow).title, QString("New fixture"));
   }

   void sortingPreservesSourceAndFiltersCompose() {
      TaskTableModel model;
      TaskFilterModel proxy;
      proxy.setSourceModel(&model);
      QAbstractItemModelTester tester(&proxy, QAbstractItemModelTester::FailureReportingMode::QtTest);
      QStringList originalTitles;
      for (int row = 0; row < model.rowCount(); ++row) {
         originalTitles.append(model.taskAt(row).title);
      }
      proxy.sort(TaskTableModel::TITLE_COLUMN, Qt::DescendingOrder);
      for (int row = 0; row < model.rowCount(); ++row) {
         QCOMPARE(model.taskAt(row).title, originalTitles.at(row));
         if (row > 0) {
            QVERIFY(QString::compare(proxy.index(row - 1, 0).data().toString(),
                                     proxy.index(row, 0).data().toString(), Qt::CaseInsensitive) >= 0);
         }
      }
      const int sourceRow = proxy.mapToSource(proxy.index(0, 0)).row();
      SampleTask changed = model.taskAt(sourceRow);
      changed.title = "A changed fixture";
      QVERIFY(model.updateTask(sourceRow, changed));
      QCOMPARE(proxy.index(proxy.rowCount() - 1, 0).data().toString(), changed.title);
      QCOMPARE(model.taskAt(sourceRow).title, changed.title);
      proxy.setNavigationTag("pillar:family");
      QCOMPARE(proxy.rowCount(), 2);
      proxy.setFilterFixedString("OUTING");
      QCOMPARE(proxy.rowCount(), 1);
      proxy.setNavigationTag("pillar:council");
      QCOMPARE(proxy.rowCount(), 0);
      proxy.setFilterFixedString("");
      QCOMPARE(proxy.rowCount(), 1);
      proxy.setNavigationTag("objective:service");
      QCOMPARE(proxy.rowCount(), 1); // One objective has exactly one organizational context.
   }

   void organizationalViewsReuseFixtureRows() {
      MainWindow window;
      auto *tree = window.findChild<QTreeView *>("navigationTree");
      auto *table = window.findChild<QTableView *>("taskTable");
      QVERIFY(tree && table);
      auto *proxy = dynamic_cast<TaskFilterModel *>(table->model());
      QVERIFY(proxy);
      auto *model = dynamic_cast<TaskTableModel *>(proxy->sourceModel());
      QVERIFY(model);
      auto *navigationModel = tree->model();
      const QModelIndex pillars = navigationModel->index(1, 0);
      const QModelIndex family = navigationModel->index(0, 0, pillars);
      const QModelIndex household = navigationModel->index(0, 0, family);
      tree->setCurrentIndex(household);
      QCOMPARE(proxy->rowCount(), 2); // Direct project task and objective task.
      QCOMPARE(proxy->index(0, 0).data().toString(), QString("Arrange family outing"));
      const QModelIndex council = navigationModel->index(1, 0, pillars);
      const QModelIndex serviceObjective = navigationModel->index(0, 0, council);
      tree->setCurrentIndex(serviceObjective);
      QCOMPARE(proxy->rowCount(), 1);
      QVERIFY(proxy->index(0, TaskTableModel::CONTEXT_COLUMN).data().toString().startsWith("Council /"));
      const QModelIndex learning = navigationModel->index(2, 0, pillars);
      const QModelIndex study = navigationModel->index(0, 0, learning);
      const QModelIndex project = navigationModel->index(0, 0, study);
      const QModelIndex subproject = navigationModel->index(0, 0, project);
      const QModelIndex objective = navigationModel->index(1, 0, project);
      tree->setCurrentIndex(subproject);
      QCOMPARE(proxy->rowCount(), 1);
      QCOMPARE(proxy->index(0, 0).data().toString(), QString("Review desktop notes"));
      tree->setCurrentIndex(objective);
      QCOMPARE(proxy->rowCount(), 1);
      const int objectiveTaskRow = proxy->mapToSource(proxy->index(0, 0)).row();
      const QModelIndex objectiveView = navigationModel->index(3, 0);
      tree->setCurrentIndex(navigationModel->index(1, 0, objectiveView));
      QCOMPARE(proxy->mapToSource(proxy->index(0, 0)).row(), objectiveTaskRow);
      const QModelIndex sprintView = navigationModel->index(4, 0);
      const QModelIndex november = navigationModel->index(0, 0, sprintView);
      const QModelIndex december = navigationModel->index(1, 0, sprintView);
      tree->setCurrentIndex(november);
      QCOMPARE(proxy->rowCount(), 2);
      tree->setCurrentIndex(navigationModel->index(0, 0, november));
      QCOMPARE(proxy->mapToSource(proxy->index(0, 0)).row(), objectiveTaskRow);
      tree->setCurrentIndex(navigationModel->index(0, 0, december));
      QCOMPARE(proxy->mapToSource(proxy->index(0, 0)).row(), objectiveTaskRow);
      SampleTask edited = model->taskAt(objectiveTaskRow);
      edited.title = "Same fixture edited once";
      QVERIFY(model->updateTask(objectiveTaskRow, edited));
      tree->setCurrentIndex(objective);
      QCOMPARE(proxy->index(0, 0).data().toString(), edited.title);
      tree->setCurrentIndex(navigationModel->index(2, 0));
      QCOMPARE(proxy->rowCount(), 1);
      QCOMPARE(proxy->index(0, 0).data().toString(), QString("Capture an idea"));
      QCOMPARE(model->rowCount(), 6); // Views never create task copies.
   }

   void dialogEditsModelAndCancelPreservesIt() {
      TaskTableModel model;
      const SampleTask original = model.taskAt(0);
      TaskEditDialog dialog(original);
      auto *title = dialog.findChild<QLineEdit *>("titleEdit");
      auto *description = dialog.findChild<QTextEdit *>("descriptionEdit");
      auto *status = dialog.findChild<QComboBox *>("statusEdit");
      auto *hasDate = dialog.findChild<QCheckBox *>("hasDueDate");
      auto *date = dialog.findChild<QDateEdit *>("dueDateEdit");
      auto *buttons = dialog.findChild<QDialogButtonBox *>();
      QVERIFY(title && description && status && hasDate && date && buttons);
      title->setText("   ");
      QVERIFY(!buttons->button(QDialogButtonBox::Ok)->isEnabled());
      title->setText(" Edited sample ");
      description->setPlainText("Updated description");
      status->setCurrentText("On Hold");
      hasDate->setChecked(true);
      date->setDate(QDate(2027, 2, 3));
      buttons->button(QDialogButtonBox::Ok)->click();
      QCOMPARE(dialog.result(), static_cast<int>(QDialog::Accepted));
      QVERIFY(model.updateTask(0, dialog.editedTask()));
      QCOMPARE(model.data(model.index(0, 0)).toString(), QString("Edited sample"));
      QCOMPARE(model.taskAt(0).description, QString("Updated description"));
      QCOMPARE(model.taskAt(0).status, QString("On Hold"));
      QCOMPARE(model.taskAt(0).dueDate, QDate(2027, 2, 3));
      TaskEditDialog cancelled(model.taskAt(0));
      cancelled.findChild<QLineEdit *>("titleEdit")->setText("Discarded");
      cancelled.reject();
      QCOMPARE(model.taskAt(0).title, QString("Edited sample"));
   }

   void windowActionsEditSortedRowAndAddTask() {
      MainWindow window;
      window.show();
      QApplication::processEvents();
      auto *table = window.findChild<QTableView *>("taskTable");
      auto *tree = window.findChild<QTreeView *>("navigationTree");
      auto *filter = window.findChild<QLineEdit *>("filterEdit");
      QVERIFY(table && tree && filter);
      auto *proxy = dynamic_cast<TaskFilterModel *>(table->model());
      QVERIFY(proxy);
      auto *model = dynamic_cast<TaskTableModel *>(proxy->sourceModel());
      QVERIFY(model);
      table->sortByColumn(TaskTableModel::TITLE_COLUMN, Qt::DescendingOrder);
      table->selectRow(0);
      const int editedRow = proxy->mapToSource(table->currentIndex()).row();
      QTimer::singleShot(0, &window, []() {
         auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
         if (dialog) {
            dialog->findChild<QLineEdit *>("titleEdit")->setText("ZZZ edited via action");
            dialog->accept();
         }
      });
      window.findChild<QAction *>("editTaskAction")->trigger();
      QCOMPARE(model->taskAt(editedRow).title, QString("ZZZ edited via action"));
      const QModelIndex pillars = tree->model()->index(1, 0);
      tree->setCurrentIndex(tree->model()->index(0, 0, pillars));
      QCOMPARE(proxy->rowCount(), 2);
      filter->setText("does not match any fixture");
      QCOMPARE(proxy->rowCount(), 0);
      QVERIFY(!window.findChild<QAction *>("editTaskAction")->isEnabled());
      QTimer::singleShot(0, &window, []() {
         auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
         if (dialog) {
            dialog->findChild<QLineEdit *>("titleEdit")->setText("Added via action");
            dialog->accept();
         }
      });
      window.findChild<QAction *>("newTaskAction")->trigger();
      QCOMPARE(model->rowCount(), 7);
      QCOMPARE(proxy->rowCount(), 7);
      QCOMPARE(model->taskAt(6).navigationTags, QStringList({"inbox"}));
      QCOMPARE(model->taskAt(6).organizationContext, QString("Inbox (independent)"));
      QCOMPARE(table->currentIndex().data().toString(), QString("Added via action"));
      window.close();
   }

   void windowLayoutRestores() {
      {
         MainWindow window;
         window.show();
         window.resize(760, 560);
         QApplication::processEvents();
         window.findChild<QSplitter *>("mainSplitter")->setSizes({220, 540});
         window.close();
      }
      QSettings settings(QSettings::IniFormat, QSettings::UserScope, "TaskCompass", "DesktopPrototype");
      QVERIFY(!settings.value("windowGeometry").toByteArray().isEmpty());
      QVERIFY(!settings.value("splitterState").toByteArray().isEmpty());
      MainWindow restored;
      restored.show();
      QApplication::processEvents();
      QCOMPARE(restored.size(), QSize(760, 560));
      QVERIFY(qAbs(restored.findChild<QSplitter *>("mainSplitter")->sizes().first() - 220) < 10);
      restored.findChild<QAction *>("navigationAction")->setChecked(false);
      QVERIFY(restored.findChild<QTreeView *>("navigationTree")->isHidden());
      restored.close();
      MainWindow hidden;
      QVERIFY(!hidden.findChild<QAction *>("navigationAction")->isChecked());
      QVERIFY(hidden.findChild<QTreeView *>("navigationTree")->isHidden());
      hidden.close();
   }

   void diagnosticWithoutConnection() {
      QProcess process;
      QProcessEnvironment environment = cleanDatabaseEnvironment();
      // Even supplied variables must not cause an implicit network connection.
      environment.insert("TASKCOMPASS_DB_HOST", "must-not-connect.invalid");
      process.setProcessEnvironment(environment);
      process.start(diagnosticExecutable, QStringList());
      QVERIFY(process.waitForFinished(10000));
      QCOMPARE(process.exitStatus(), QProcess::NormalExit);
      QCOMPARE(process.exitCode(), 0);
      const QByteArray output = process.readAllStandardOutput();
      QVERIFY(output.contains("Qt SQL drivers:"));
      QVERIFY(output.contains("QMYSQL available:"));
      QVERIFY(output.contains("Connection skipped:"));
      QVERIFY(process.readAllStandardError().isEmpty());
   }

   void diagnosticRejectsIncompleteConfiguration() {
      QProcess process;
      process.setProcessEnvironment(cleanDatabaseEnvironment());
      process.start(diagnosticExecutable, {"--connect"});
      QVERIFY(process.waitForFinished(10000));
      QCOMPARE(process.exitCode(), 3);
      QVERIFY(process.readAllStandardError().contains("Configuration error:"));
   }

private:
   QProcessEnvironment cleanDatabaseEnvironment() const {
      QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
      for (const QString &key : environment.keys()) {
         if (key.startsWith("TASKCOMPASS_DB_")) {
            environment.remove(key);
         }
      }
      return environment;
   }

   QString diagnosticExecutable;
};

int main(int argc, char *argv[]) {
   QApplication application(argc, argv);
   if (argc != 2) {
      return 1;
   }
   QTemporaryDir settingsDirectory;
   if (!settingsDirectory.isValid()) {
      return 1;
   }
   QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDirectory.path());
   PrototypeTests tests(QString::fromLocal8Bit(argv[1]));
   return QTest::qExec(&tests, 1, argv);
}

#include "PrototypeTests.moc"
