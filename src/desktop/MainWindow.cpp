#include "MainWindow.h"
#include "dialogs/TaskEditDialog.h"
#include "models/TaskFilterModel.h"
#include "models/TaskTableModel.h"

#include <QAction>
#include <QCloseEvent>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QSplitter>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QTableView>
#include <QToolBar>
#include <QTreeView>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
   : QMainWindow(parent), settings(QSettings::IniFormat, QSettings::UserScope,
                                   "TaskCompass", "DesktopPrototype") {
   setWindowTitle("TaskCompass — Phase 0.1 prototype (memory only)");
   resize(1100, 700);
   splitter = new QSplitter(Qt::Horizontal, this);
   splitter->setObjectName("mainSplitter");
   navigation = new QTreeView(splitter);
   navigation->setObjectName("navigationTree");
   navigation->setEditTriggers(QAbstractItemView::NoEditTriggers);
   navigation->setMinimumWidth(150);
   createNavigation();

   auto *taskPanel = new QWidget(splitter);
   auto *layout = new QVBoxLayout(taskPanel);
   contextLabel = new QLabel("All sample tasks", taskPanel);
   filterEdit = new QLineEdit(taskPanel);
   filterEdit->setObjectName("filterEdit");
   filterEdit->setPlaceholderText("Filter sample tasks across all columns (Ctrl+F)");
   filterEdit->setClearButtonEnabled(true);
   table = new QTableView(taskPanel);
   table->setObjectName("taskTable");
   taskModel = new TaskTableModel(this);
   filterModel = new TaskFilterModel(this);
   filterModel->setSourceModel(taskModel);
   table->setModel(filterModel);
   table->setSelectionBehavior(QAbstractItemView::SelectRows);
   table->setSelectionMode(QAbstractItemView::SingleSelection);
   table->setEditTriggers(QAbstractItemView::NoEditTriggers);
   table->setAlternatingRowColors(true);
   table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
   table->horizontalHeader()->setStretchLastSection(true);
   table->setSortingEnabled(true);
   table->sortByColumn(TaskTableModel::TITLE_COLUMN, Qt::AscendingOrder);
   table->setColumnWidth(TaskTableModel::TITLE_COLUMN, 290);
   table->setColumnWidth(TaskTableModel::DUE_DATE_COLUMN, 110);
   layout->addWidget(contextLabel);
   layout->addWidget(filterEdit);
   layout->addWidget(table);
   splitter->setStretchFactor(0, 0);
   splitter->setStretchFactor(1, 1);
   splitter->setSizes({240, 860});
   setCentralWidget(splitter);
   createActions();

   connect(filterEdit, &QLineEdit::textChanged, filterModel, &QSortFilterProxyModel::setFilterFixedString);
   connect(navigation->selectionModel(), &QItemSelectionModel::currentChanged,
           this, [this](const QModelIndex &current, const QModelIndex &) {
      currentNavigationTag = current.data(Qt::UserRole + 1).toString();
      contextLabel->setText(current.data().toString() + " — sample navigation");
      filterModel->setNavigationTag(currentNavigationTag);
      updateStatus();
   });
   connect(table, &QTableView::doubleClicked, this, [this](const QModelIndex &) {
      editTask();
   });
   connect(table->selectionModel(), &QItemSelectionModel::selectionChanged,
           this, [this]() { updateStatus(); });
   connect(filterModel, &QAbstractItemModel::modelReset, this, &MainWindow::updateStatus);
   connect(filterModel, &QAbstractItemModel::rowsInserted,
           this, [this]() { updateStatus(); });
   connect(filterModel, &QAbstractItemModel::rowsRemoved,
           this, [this]() { updateStatus(); });

   restoreGeometry(settings.value("windowGeometry").toByteArray());
   splitter->restoreState(settings.value("splitterState").toByteArray());
   navigationAction->setChecked(settings.value("showNavigation", true).toBool());
   navigation->setVisible(navigationAction->isChecked());
   navigation->setCurrentIndex(navigation->model()->index(0, 0));
   if (filterModel->rowCount() > 0) {
      table->selectRow(0);
   }
   updateStatus();
}

void MainWindow::createNavigation() {
   auto *model = new QStandardItemModel(navigation);
   model->setHorizontalHeaderLabels({"Prototype navigation"});
   auto addItem = [](QStandardItem *parent, const QString &label, const QString &tag) {
      auto *item = new QStandardItem(label);
      item->setData(tag, Qt::UserRole + 1);
      parent->appendRow(item);
      return item;
   };
   auto *root = model->invisibleRootItem();
   addItem(root, "All sample tasks", "");
   auto *pillars = addItem(root, "Pillars", "pillars");
   auto *family = addItem(pillars, "Family", "pillar:family");
   auto *household = addItem(family, "Household (project)", "project:household");
   addItem(household, "Family outing (objective)", "objective:familyOuting");
   auto *council = addItem(pillars, "Council", "pillar:council");
   addItem(council, "Service focus (objective)", "objective:service");
   auto *learning = addItem(pillars, "Learning", "pillar:learning");
   auto *study = addItem(learning, "Study (subpillar)", "pillar:study");
   auto *taskCompass = addItem(study, "TaskCompass (project)", "project:taskcompass");
   addItem(taskCompass, "Desktop notes (subproject)", "project:notes");
   addItem(taskCompass, "Complete desktop prototype (objective)", "objective:prototype");
   addItem(root, "Inbox (independent)", "inbox");

   // Alternate view nodes reuse fixture keys and the same task model rows.
   auto *objectives = addItem(root, "Objectives (alternate view)", "objectives");
   addItem(objectives, "Service focus — Council", "objective:service");
   addItem(objectives, "Complete desktop prototype — Learning / Study / TaskCompass", "objective:prototype");
   addItem(objectives, "Family outing — Family / Household", "objective:familyOuting");
   auto *sprints = addItem(root, "Sprints (fixture selections)", "sprints");
   auto *november = addItem(sprints, "November 2026", "sprint:november2026");
   addItem(november, "Complete desktop prototype — TaskCompass", "objective:prototype");
   addItem(november, "Service focus — Council", "objective:service");
   auto *december = addItem(sprints, "December 2026", "sprint:december2026");
   addItem(december, "Complete desktop prototype — TaskCompass", "objective:prototype");
   navigation->setModel(model);
   navigation->expandAll();
}

void MainWindow::createActions() {
   auto *fileMenu = menuBar()->addMenu("&File");
   auto *exitAction = fileMenu->addAction("E&xit");
   exitAction->setShortcut(QKeySequence("Ctrl+Q"));
   connect(exitAction, &QAction::triggered, this, &QWidget::close);
   auto *taskMenu = menuBar()->addMenu("&Task");
   auto *newAction = taskMenu->addAction("&New Task...");
   newAction->setObjectName("newTaskAction");
   newAction->setShortcut(QKeySequence::New);
   connect(newAction, &QAction::triggered, this, &MainWindow::newTask);
   editAction = taskMenu->addAction("&Edit Task...");
   editAction->setObjectName("editTaskAction");
   editAction->setShortcut(QKeySequence("Ctrl+E"));
   connect(editAction, &QAction::triggered, this, &MainWindow::editTask);
   auto *viewMenu = menuBar()->addMenu("&View");
   navigationAction = viewMenu->addAction("Show &Navigation");
   navigationAction->setObjectName("navigationAction");
   navigationAction->setCheckable(true);
   navigationAction->setChecked(true);
   navigationAction->setShortcut(QKeySequence("Ctrl+Shift+N"));
   connect(navigationAction, &QAction::toggled, navigation, &QWidget::setVisible);
   auto *filterAction = viewMenu->addAction("&Filter Tasks");
   filterAction->setShortcut(QKeySequence::Find);
   connect(filterAction, &QAction::triggered, this, [this]() {
      filterEdit->setFocus();
      filterEdit->selectAll();
   });
   auto *helpMenu = menuBar()->addMenu("&Help");
   auto *aboutAction = helpMenu->addAction("&About");
   aboutAction->setShortcut(QKeySequence("F1"));
   connect(aboutAction, &QAction::triggered, this, [this]() {
      QMessageBox::about(this, "About TaskCompass prototype",
                         "Phase 0.1 — C++20 / Qt 6 Widgets technology prototype.\n"
                         "Tasks are disposable in-memory samples. No scheduling,\n"
                         "task persistence or synchronization is implemented.\n"
                         "Only window layout preferences are saved.");
   });
   auto *toolbar = addToolBar("Sample tasks");
   toolbar->setObjectName("taskToolbar");
   toolbar->addAction(newAction);
   toolbar->addAction(editAction);
   toolbar->addSeparator();
   toolbar->addAction(navigationAction);
}

void MainWindow::newTask() {
   SampleTask task;
   task.organizationContext = "Inbox (independent)";
   task.navigationTags = {"inbox"};
   TaskEditDialog dialog(task, this);
   dialog.setWindowTitle("New sample task — memory only");
   if (dialog.exec() == QDialog::Accepted) {
      const int row = taskModel->appendTask(dialog.editedTask());
      filterEdit->clear();
      navigation->setCurrentIndex(navigation->model()->index(0, 0));
      selectSourceRow(row);
      updateStatus();
   }
}

void MainWindow::editTask() {
   const QModelIndex current = table->currentIndex();
   if (!current.isValid()) {
      return;
   }
   const int row = filterModel->mapToSource(current).row();
   TaskEditDialog dialog(taskModel->taskAt(row), this);
   if (dialog.exec() == QDialog::Accepted) {
      taskModel->updateTask(row, dialog.editedTask());
      selectSourceRow(row);
      updateStatus();
   }
}

void MainWindow::selectSourceRow(int row) {
   const QModelIndex visibleIndex = filterModel->mapFromSource(taskModel->index(row, 0));
   if (visibleIndex.isValid()) {
      table->selectRow(visibleIndex.row());
      table->scrollTo(visibleIndex);
   }
}

void MainWindow::updateStatus() {
   editAction->setEnabled(table->selectionModel()->hasSelection());
   statusBar()->showMessage(QString("%1 of %2 sample tasks — edits are lost on exit")
                            .arg(filterModel->rowCount()).arg(taskModel->rowCount()));
}

void MainWindow::closeEvent(QCloseEvent *event) {
   settings.setValue("windowGeometry", saveGeometry());
   settings.setValue("splitterState", splitter->saveState());
   settings.setValue("showNavigation", navigationAction->isChecked());
   settings.sync();
   if (settings.status() != QSettings::NoError) {
      statusBar()->showMessage("Could not save window layout preferences.");
   }
   QMainWindow::closeEvent(event);
}
