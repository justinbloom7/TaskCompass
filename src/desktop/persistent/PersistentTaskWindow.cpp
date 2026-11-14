#include "PersistentTaskWindow.h"
#include "PersistentTaskModel.h"
#include "PersistentTaskFilter.h"

#include <QAction>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QTableView>
#include <QTextEdit>
#include <QTimer>
#include <QToolBar>
#include <QTreeView>
#include <QVBoxLayout>

PersistentTaskWindow::PersistentTaskWindow(PersistentTaskController::SessionFactory factory, QWidget *parent)
   : QMainWindow(parent), settings(QSettings::IniFormat, QSettings::UserScope, "TaskCompass", "Desktop"),
     controller(new PersistentTaskController(std::move(factory), this)) {
   setWindowTitle("TaskCompass — Persistent Task Foundation");
   resize(1100, 700);
   splitter = new QSplitter(Qt::Horizontal, this);
   navigation = new QTreeView(splitter);
   auto *navigationModel = new QStandardItemModel(navigation);
   navigationModel->setHorizontalHeaderLabels({"Tasks"});
   navigationModel->appendRow(new QStandardItem("Inbox — independent tasks"));
   navigation->setModel(navigationModel);
   navigation->setEditTriggers(QAbstractItemView::NoEditTriggers);
   auto *panel = new QWidget(splitter);
   auto *layout = new QVBoxLayout(panel);
   layout->addWidget(new QLabel("Independent tasks — organizational management arrives in Phase 1.2", panel));
   search = new QLineEdit(panel);
   search->setObjectName("searchEdit");
   search->setPlaceholderText("Filter tasks (Ctrl+F)");
   search->setClearButtonEnabled(true);
   showCompleted = new QCheckBox("Show completed tasks", panel);
   showCompleted->setObjectName("showCompleted");
   layout->addWidget(search);
   layout->addWidget(showCompleted);
   table = new QTableView(panel);
   table->setObjectName("persistentTaskTable");
   model = new PersistentTaskModel(this);
   auto *activeProxy = new PersistentTaskFilter;
   activeProxy->setParent(this);
   proxy = activeProxy;
   proxy->setSourceModel(model);
   proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
   proxy->setFilterKeyColumn(-1);
   table->setModel(proxy);
   table->setSelectionBehavior(QAbstractItemView::SelectRows);
   table->setSelectionMode(QAbstractItemView::SingleSelection);
   table->setEditTriggers(QAbstractItemView::NoEditTriggers);
   table->setAlternatingRowColors(true);
   table->setSortingEnabled(true);
   table->sortByColumn(0, Qt::AscendingOrder);
   table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
   table->horizontalHeader()->setStretchLastSection(true);
   table->setColumnWidth(0, 300);
   layout->addWidget(table);
   splitter->setSizes({240, 860});
   splitter->setStretchFactor(1, 1);
   setCentralWidget(splitter);

   auto *file = menuBar()->addMenu("&File");
   file->addAction("E&xit", QKeySequence("Ctrl+Q"), this, &QWidget::close);
   auto *edit = menuBar()->addMenu("&Edit");
   undoAction = edit->addAction("&Undo", QKeySequence::Undo, controller, &PersistentTaskController::undo);
   undoAction->setObjectName("undoAction");
   redoAction = edit->addAction("&Redo", QKeySequence("Ctrl+Shift+Z"), controller, &PersistentTaskController::redo);
   redoAction->setObjectName("redoAction");
   auto *taskMenu = menuBar()->addMenu("&Task");
   newAction = taskMenu->addAction("&New Task...", QKeySequence::New, this, [this]() { editTask(true); });
   newAction->setObjectName("newTaskAction");
   editAction = taskMenu->addAction("&Edit Task...", QKeySequence("Ctrl+E"), this, [this]() { editTask(false); });
   editAction->setObjectName("editTaskAction");
   completeAction = taskMenu->addAction("Complete Task", this, [this]() {
      if (const auto task = selectedTask()) {
         setCompleted(QString::fromStdString(task->id), task->status == taskcompass::TaskStatus::ACTIVE);
      }
   });
   completeAction->setObjectName("completeTaskAction");
   auto *view = menuBar()->addMenu("&View");
   navigationAction = view->addAction("Show &Navigation");
   navigationAction->setCheckable(true);
   navigationAction->setChecked(true);
   navigationAction->setShortcut(QKeySequence("Ctrl+Shift+N"));
   connect(navigationAction, &QAction::toggled, navigation, &QWidget::setVisible);
   view->addAction("&Filter Tasks", QKeySequence::Find, search, qOverload<>(&QWidget::setFocus));
   reloadAction = view->addAction("&Reload", QKeySequence("F5"), controller, &PersistentTaskController::reload);
   auto *help = menuBar()->addMenu("&Help");
   help->addAction("&About", QKeySequence("F1"), this, [this]() {
      QMessageBox::about(this, "TaskCompass", "Phase 1.1 — persistent independent tasks.\nGPL-3.0-or-later.\nSession undo does not survive restart.\nPlanning and synchronization are deferred.");
   });
   auto *toolbar = addToolBar("Tasks");
   toolbar->setObjectName("persistentToolbar");
   toolbar->addActions({newAction, editAction, completeAction, undoAction, redoAction});
   connect(search, &QLineEdit::textChanged, proxy, &QSortFilterProxyModel::setFilterFixedString);
   connect(showCompleted, &QCheckBox::toggled, this, [activeProxy](bool enabled) {
      activeProxy->setIncludeCompleted(enabled);
   });
   connect(table, &QTableView::doubleClicked, this, [this](const QModelIndex &index) {
      if (index.column() != PersistentTaskModel::STATUS_COLUMN) {
         editTask(false);
      }
   });
   connect(model, &PersistentTaskModel::completionRequested, this, &PersistentTaskWindow::setCompleted);
   connect(table->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]() { refresh(); });
   connect(controller, &PersistentTaskController::changed, this, &PersistentTaskWindow::refresh);
   connect(controller, &PersistentTaskController::failed, this, [this](const QString &message) {
      QMessageBox::warning(this, "TaskCompass database/validation error", message + "\nUse View → Reload after resolving the problem.");
   });
   restoreGeometry(settings.value("windowGeometry").toByteArray());
   splitter->restoreState(settings.value("splitterState").toByteArray());
   navigationAction->setChecked(settings.value("showNavigation", true).toBool());
   navigation->setVisible(navigationAction->isChecked());
   refresh();
   QTimer::singleShot(0, controller, &PersistentTaskController::reload);
}

std::optional<taskcompass::Task> PersistentTaskWindow::selectedTask() const {
   const auto index = proxy->mapToSource(table->currentIndex());
   if (!index.isValid()) {
      return {};
   }
   return model->taskAt(index.row());
}

void PersistentTaskWindow::refresh() {
   // Avoid resetting selection merely because the worker became busy or a filter changed.
   const auto selected = selectedTask();
   if (controller->tasks() != [this]() {
      std::vector<taskcompass::Task> current;
      for (int row = 0; row < model->rowCount(); ++row) {
         current.push_back(model->taskAt(row));
      }
      return current;
   }()) {
      const QSignalBlocker block(table->selectionModel());
      model->replaceTasks(controller->tasks());
      for (int row = 0; row < model->rowCount(); ++row) {
         if (selected && model->taskAt(row).id == selected->id) {
            const auto visible = proxy->mapFromSource(model->index(row, 0));
            if (visible.isValid()) {
               table->selectRow(visible.row());
            }
         }
      }
   }
   const bool enabled = controller->isReady() && !controller->isBusy();
   const auto task = selectedTask();
   model->setWritable(enabled);
   newAction->setEnabled(enabled);
   editAction->setEnabled(enabled && task.has_value());
   completeAction->setEnabled(enabled && task.has_value());
   completeAction->setText(task && task->status == taskcompass::TaskStatus::COMPLETED ? "Reopen Task" : "Complete Task");
   undoAction->setText("&Undo " + controller->undoDescription());
   redoAction->setText("&Redo " + controller->redoDescription());
   undoAction->setEnabled(controller->canUndo());
   redoAction->setEnabled(controller->canRedo());
   reloadAction->setEnabled(!controller->isBusy());
   statusBar()->showMessage(controller->isBusy() ? "Working..." : enabled
                            ? QString("%1 visible of %2 tasks — changes persist immediately").arg(proxy->rowCount()).arg(model->rowCount())
                            : "Database unavailable — resolve the error, then Reload");
}

void PersistentTaskWindow::editTask(bool create) {
   const auto original = create ? std::optional<taskcompass::Task>() : selectedTask();
   if (!create && !original) {
      return;
   }
   QDialog dialog(this);
   dialog.setWindowTitle(create ? "New Task" : "Edit Task");
   dialog.resize(480, 360);
   auto *layout = new QVBoxLayout(&dialog);
   auto *form = new QFormLayout;
   auto *title = new QLineEdit(original ? QString::fromStdString(original->title) : QString(), &dialog);
   title->setObjectName("titleEdit");
   auto *notes = new QTextEdit(&dialog);
   notes->setObjectName("notesEdit");
   notes->setAcceptRichText(false);
   notes->setPlainText(original ? QString::fromStdString(original->notes.value_or("")) : QString());
   form->addRow("&Title:", title);
   form->addRow("&Notes:", notes);
   layout->addLayout(form);
   auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
   buttons->button(QDialogButtonBox::Ok)->setEnabled(!title->text().trimmed().isEmpty());
   connect(title, &QLineEdit::textChanged, buttons, [buttons](const QString &value) {
      buttons->button(QDialogButtonBox::Ok)->setEnabled(!value.trimmed().isEmpty());
   });
   connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
   connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
   layout->addWidget(buttons);
   if (dialog.exec() != QDialog::Accepted) {
      return;
   }
   std::optional<std::string> value;
   if (!notes->toPlainText().isEmpty()) {
      value = notes->toPlainText().toStdString();
   } else if (original && original->notes && original->notes->empty()) {
      value = "";
   }
   controller->execute({create ? taskcompass::TaskOperation::CREATE : taskcompass::TaskOperation::EDIT,
                        original ? original->id : "", original ? original->localRevision : 0,
                        title->text().toStdString(), value});
}

void PersistentTaskWindow::setCompleted(const QString &id, bool completed) {
   for (const auto &task : controller->tasks()) {
      if (task.id == id.toStdString()) {
         controller->execute({completed ? taskcompass::TaskOperation::COMPLETE : taskcompass::TaskOperation::REOPEN,
                              task.id, task.localRevision, {}, {}});
         return;
      }
   }
}

void PersistentTaskWindow::closeEvent(QCloseEvent *event) {
   if (controller->isBusy()) {
      statusBar()->showMessage("Wait for the current operation before closing.");
      event->ignore();
      return;
   }
   settings.setValue("windowGeometry", saveGeometry());
   settings.setValue("splitterState", splitter->saveState());
   settings.setValue("showNavigation", navigationAction->isChecked());
   settings.sync();
   QMainWindow::closeEvent(event);
}
