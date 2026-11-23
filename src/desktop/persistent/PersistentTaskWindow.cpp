#include "PersistentTaskWindow.h"
#include "PersistentTaskModel.h"
#include "PersistentTaskFilter.h"
#include "desktop/models/OrganizationTreeModel.h"
#include "desktop/dialogs/OrganizationEditDialog.h"
#include "desktop/dialogs/OrganizationMoveDialog.h"

#include <QAction>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QStyle>
#include <QTableView>
#include <QTextEdit>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>
#include <QComboBox>
#include <QPushButton>
#include <QMenu>
#include <algorithm>

namespace {
QString subtreeImpact(const taskcompass::WorkspaceSnapshot &snapshot, const taskcompass::OrganizationKey &root) {
   if (snapshot.organizations.empty()) { return {}; }
   taskcompass::OrganizationRules graph(taskcompass::organizationFields(snapshot.organizations.front()).datasetId, snapshot.organizations);
   auto includes = [&](const taskcompass::OrganizationKey &key) {
      const auto path = graph.path(key);
      return std::find(path.begin(), path.end(), root) != path.end();
   };
   std::size_t descendants = 0, tasks = 0, completed = 0;
   for (const auto &entity : snapshot.organizations) {
      const auto key = taskcompass::organizationKey(entity);
      if (!taskcompass::organizationFields(entity).deletedAt && key != root && includes(key)) { ++descendants; }
   }
   for (const auto &task : snapshot.tasks) {
      if (task.deletedAt || task.owner.kind == taskcompass::TaskOwnerKind::INDEPENDENT) { continue; }
      const taskcompass::OrganizationKey owner{task.owner.kind == taskcompass::TaskOwnerKind::OBJECTIVE
         ? taskcompass::OrganizationKind::OBJECTIVE : taskcompass::OrganizationKind::PROJECT, task.owner.id};
      if (includes(owner)) { ++tasks; if (task.status == taskcompass::TaskStatus::COMPLETED) { ++completed; } }
   }
   return QString("Subtree: %1 descendant organizations and %2 tasks (%3 completed), including archived items; independent of current filters.")
      .arg(static_cast<qulonglong>(descendants)).arg(static_cast<qulonglong>(tasks)).arg(static_cast<qulonglong>(completed));
}
}

PersistentTaskWindow::PersistentTaskWindow(PersistentTaskController::SessionFactory factory, QWidget *parent)
   : QMainWindow(parent), settings(QSettings::IniFormat, QSettings::UserScope, "TaskCompass", "Desktop"),
     controller(new PersistentTaskController(std::move(factory), this)) {
   setWindowTitle("TaskCompass");
   resize(1100, 700);
   splitter = new QSplitter(Qt::Horizontal, this);
   navigation = new QTreeView(splitter);
   organizationModel = new OrganizationTreeModel(navigation);
   organizationModel->replace({}, false, false);
   navigation->setObjectName("organizationTree");
   navigation->setModel(organizationModel);
   navigation->setEditTriggers(QAbstractItemView::NoEditTriggers);
   auto *panel = new QWidget(splitter);
   auto *layout = new QVBoxLayout(panel);
   selectionDetails = new QLabel("All tasks", panel);
   selectionDetails->setObjectName("selectionDetails");
   selectionDetails->setWordWrap(true);
   layout->addWidget(selectionDetails);
   navigationMode = new QComboBox(panel);
   navigationMode->setObjectName("navigationMode");
   navigationMode->addItems({"Project-oriented navigation", "Objective-oriented navigation"});
   navigationMode->setCurrentIndex(settings.value("objectiveView", false).toBool() ? 1 : 0);
   layout->addWidget(navigationMode);
   search = new QLineEdit(panel);
   search->setObjectName("searchEdit");
   search->setPlaceholderText("Filter tasks (Ctrl+F)");
   search->setClearButtonEnabled(true);
   showCompleted = new QCheckBox("Show completed tasks", panel);
   showCompleted->setObjectName("showCompleted");
   showArchived = new QCheckBox("Show archived organization and tasks", panel);
   showArchived->setObjectName("showArchived");
   showArchived->setChecked(false); // Session-only: ignore historical saved values.
   layout->addWidget(search);
   layout->addWidget(showCompleted);
   layout->addWidget(showArchived);
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
   table->setWordWrap(false);
   const int compactRowHeight = std::max(table->fontMetrics().height(), table->style()->pixelMetric(QStyle::PM_IndicatorHeight)) + 6;
   table->verticalHeader()->setMinimumSectionSize(compactRowHeight);
   table->verticalHeader()->setDefaultSectionSize(compactRowHeight);
   table->setSortingEnabled(true);
   table->sortByColumn(0, Qt::AscendingOrder);
   table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
   table->horizontalHeader()->setStretchLastSection(true);
   table->setColumnWidth(0, 300);
   table->setColumnHidden(PersistentTaskModel::CONTEXT_COLUMN, true);
   table->setColumnHidden(PersistentTaskModel::PRIORITY_COLUMN, true);
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
   assignContextAction = taskMenu->addAction("Set Context...", this, &PersistentTaskWindow::assignContext);
   assignContextAction->setObjectName("assignContextAction");
   priorityAction = taskMenu->addAction("Set Priority...", this, &PersistentTaskWindow::editPriority);
   priorityAction->setObjectName("taskPriorityAction");
   auto *contexts = menuBar()->addMenu("&Contexts");
   contextMenuAction = contexts->menuAction();
   using Operation = taskcompass::ContextOperation;
   for (const auto &[label, operation] : {std::pair{"New Context...", Operation::CREATE}, {"Rename...", Operation::RENAME},
                                          {"Archive...", Operation::ARCHIVE}, {"Restore...", Operation::RESTORE},
                                          {"Delete...", Operation::DELETE_CONTEXT}, {"Move Earlier...", Operation::MOVE_UP},
                                          {"Move Later...", Operation::MOVE_DOWN}}) {
      contexts->addAction(label, this, [this, operation]() { changeContext(operation); });
   }
   auto *organize = menuBar()->addMenu("&Organize");
   for (const auto kind : {taskcompass::OrganizationKind::PILLAR, taskcompass::OrganizationKind::PROJECT, taskcompass::OrganizationKind::OBJECTIVE}) {
      const QString name = kind == taskcompass::OrganizationKind::PILLAR ? "Pillar" : kind == taskcompass::OrganizationKind::PROJECT ? "Project" : "Objective";
      auto *action = organize->addAction("New " + name + "...", this, [this, kind]() { editOrganization(kind); });
      action->setObjectName("new" + name + "Action"); organizationActions.push_back(action);
   }
   auto addOrganizationAction = [&](const QString &label, const QString &name, auto handler) {
      auto *action = organize->addAction(label, this, handler); action->setObjectName(name); organizationActions.push_back(action);
   };
   addOrganizationAction("Edit / Rename...", "editOrganizationAction", [this]() { editOrganization({}); });
   addOrganizationAction("Move...", "moveOrganizationAction", [this]() { moveOrganization(); });
   addOrganizationAction("Archive...", "archiveOrganizationAction", [this]() { archiveOrganization(false); });
   addOrganizationAction("Restore...", "restoreOrganizationAction", [this]() { archiveOrganization(true); });
   addOrganizationAction("New Task Under Selection...", "newOwnedTaskAction", [this]() {
      if (const auto entity = selectedOrganization()) {
         const auto key = taskcompass::organizationKey(*entity);
         if (key.kind != taskcompass::OrganizationKind::PILLAR) {
            editTask(true, taskcompass::TaskOwner{key.kind == taskcompass::OrganizationKind::OBJECTIVE ? taskcompass::TaskOwnerKind::OBJECTIVE : taskcompass::TaskOwnerKind::PROJECT, key.id});
         }
      }
   });
   addOrganizationAction("Locate Archived Ancestor", "locateArchivedAncestorAction", [this]() {
      if (const auto entity = selectedOrganization()) {
         taskcompass::OrganizationRules graph(taskcompass::organizationFields(*entity).datasetId, controller->workspace().organizations);
         for (const auto &ancestor : graph.path(taskcompass::organizationKey(*entity))) {
            if (taskcompass::organizationFields(graph.find(ancestor)).archivedAt) {
               navigationMode->setCurrentIndex(0);
               const auto index = organizationModel->indexFor(ancestor);
               navigation->setCurrentIndex(index); navigation->scrollTo(index); break;
            }
         }
      }
   });
   navigation->setContextMenuPolicy(Qt::CustomContextMenu);
   connect(navigation, &QWidget::customContextMenuRequested, this, [this](const QPoint &point) {
      const auto index = navigation->indexAt(point);
      navigation->setCurrentIndex(index);
      refresh(); QMenu menu(this);
      populateCreationMenu(&menu);
      menu.addSeparator();
      for (std::size_t i = 3; i < organizationActions.size(); ++i) { menu.addAction(organizationActions[i]); }
      menu.exec(navigation->viewport()->mapToGlobal(point));
   });
   auto *view = menuBar()->addMenu("&View");
   groupByContextAction = view->addAction("Group by Context", this, [this]() {
      table->sortByColumn(PersistentTaskModel::CONTEXT_COLUMN, Qt::AscendingOrder);
   });
   navigationAction = view->addAction("Show &Navigation");
   navigationAction->setCheckable(true);
   navigationAction->setChecked(true);
   navigationAction->setShortcut(QKeySequence("Ctrl+Shift+N"));
   connect(navigationAction, &QAction::toggled, navigation, &QWidget::setVisible);
   view->addAction("&Filter Tasks", QKeySequence::Find, search, qOverload<>(&QWidget::setFocus));
   reloadAction = view->addAction("&Reload", QKeySequence("F5"), controller, &PersistentTaskController::reload);
   connect(controller, &PersistentTaskController::commitResolved, this, [this](const QString &message) {
      statusBar()->showMessage(message);
   });
   auto *help = menuBar()->addMenu("&Help");
   help->addAction("&About", QKeySequence("F1"), this, [this]() {
      QMessageBox::about(this, "TaskCompass", "Persistent tasks and organization.\nGPL-3.0-or-later.\nSession undo does not survive restart.\nPlanning and synchronization are deferred.");
   });
   auto *toolbar = addToolBar("Tasks");
   toolbar->setObjectName("persistentToolbar");
   auto *createButton = new QToolButton(toolbar);
   createButton->setObjectName("createButton"); createButton->setText("+");
   createButton->setToolTip("Create under the current selection");
   createButton->setAccessibleName("Create");
   createButton->setPopupMode(QToolButton::InstantPopup);
   auto *creationMenu = new QMenu(createButton); creationMenu->setObjectName("creationMenu");
   connect(creationMenu, &QMenu::aboutToShow, this, [this, creationMenu]() { populateCreationMenu(creationMenu); });
   createButton->setMenu(creationMenu); toolbar->addWidget(createButton);
   toolbar->addActions({editAction, completeAction, undoAction, redoAction});
   navigation->setDragDropMode(QAbstractItemView::DragDrop);
   navigation->setDefaultDropAction(Qt::MoveAction);
   navigation->setDragDropOverwriteMode(true);
   connect(organizationModel, &OrganizationTreeModel::reparentRequested, this, [this](const QModelIndex &source, const QModelIndex &destination) {
      const auto key = organizationModel->keyAt(source), parent = organizationModel->keyAt(destination);
      if (!key || !parent) { return; }
      // Defer modal interaction until Qt finishes delivering the drop event.
      QTimer::singleShot(0, this, [this, key, parent]() {
         if (!controller->isReady() || controller->isBusy()) { return; }
         const auto index = organizationModel->indexFor(*key);
         if (!index.isValid()) { return; }
         navigation->setCurrentIndex(index); moveOrganization(parent);
      });
   });
   connect(search, &QLineEdit::textChanged, proxy, &QSortFilterProxyModel::setFilterFixedString);
   connect(showCompleted, &QCheckBox::toggled, this, [activeProxy](bool enabled) {
      activeProxy->setIncludeCompleted(enabled);
   });
   connect(showArchived, &QCheckBox::toggled, this, [this](bool) { rebuildNavigation(); updateScope(); refresh(); });
   connect(navigationMode, &QComboBox::currentIndexChanged, this, [this](int) { rebuildNavigation(); updateScope(); refresh(); });
   connect(navigation->selectionModel(), &QItemSelectionModel::currentChanged, this, [this]() { updateScope(); refresh(); });
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
   if (controller->isReady() && (!navigationRestored || controller->workspace() != renderedWorkspace)) {
      const QSignalBlocker block(table->selectionModel());
      const bool enableGrouping = renderedWorkspace.schemaVersion != 3 && controller->workspace().schemaVersion == 3;
      renderedWorkspace = controller->workspace();
      model->replaceWorkspace(renderedWorkspace);
      table->setColumnHidden(PersistentTaskModel::CONTEXT_COLUMN, renderedWorkspace.schemaVersion != 3);
      table->setColumnHidden(PersistentTaskModel::PRIORITY_COLUMN, renderedWorkspace.schemaVersion != 3);
      if (enableGrouping) { table->sortByColumn(PersistentTaskModel::CONTEXT_COLUMN, Qt::AscendingOrder); }
      rebuildNavigation(); updateScope();
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
   organizationModel->setReparentingEnabled(enabled && navigationMode->currentIndex() == 0);
   const bool contextsEnabled = enabled && renderedWorkspace.schemaVersion == 3;
   contextMenuAction->setEnabled(contextsEnabled);
   groupByContextAction->setEnabled(contextsEnabled);
   assignContextAction->setEnabled(contextsEnabled && task.has_value());
   priorityAction->setEnabled(contextsEnabled && task.has_value());
   newAction->setEnabled(enabled);
   editAction->setEnabled(enabled && task.has_value());
   completeAction->setEnabled(enabled && task.has_value());
   completeAction->setText(task && task->status == taskcompass::TaskStatus::COMPLETED ? "Reopen Task" : "Complete Task");
   undoAction->setText("&Undo " + controller->undoDescription());
   redoAction->setText("&Redo " + controller->redoDescription());
   undoAction->setEnabled(controller->canUndo());
   redoAction->setEnabled(controller->canRedo());
   reloadAction->setEnabled(!controller->isBusy());
   const auto organization = selectedOrganization();
   selectionDetails->setText(organization ? navigation->currentIndex().data(OrganizationTreeModel::TYPE_ROLE).toString()
      + " — " + navigation->currentIndex().data(OrganizationTreeModel::PATH_ROLE).toString() : "All tasks / Independent inbox");
   selectionDetails->setToolTip(organization ? QString::fromStdString(taskcompass::organizationFields(*organization).id) : QString());
   for (std::size_t index = 0; index < organizationActions.size(); ++index) {
      bool allowed = enabled && (index < 3 || organization.has_value());
      if (organization && index == 5) { allowed = allowed && !taskcompass::organizationFields(*organization).archivedAt; }
      if (organization && index == 6) { allowed = allowed && taskcompass::organizationFields(*organization).archivedAt.has_value(); }
      if (organization && index == 7) {
         allowed = allowed && taskcompass::organizationKey(*organization).kind != taskcompass::OrganizationKind::PILLAR
                   && !navigation->currentIndex().data(OrganizationTreeModel::EFFECTIVE_ARCHIVE_ROLE).toBool();
      }
      if (organization && index == 8) { allowed = allowed && navigation->currentIndex().data(OrganizationTreeModel::EFFECTIVE_ARCHIVE_ROLE).toBool(); }
      organizationActions[index]->setEnabled(allowed);
   }
   statusBar()->showMessage(controller->isBusy() ? "Working..." : enabled
                            ? QString("%1 visible of %2 tasks — changes persist immediately").arg(proxy->rowCount()).arg(model->rowCount())
                            : "Database unavailable — resolve the error, then Reload");
}

void PersistentTaskWindow::editTask(bool create, std::optional<taskcompass::TaskOwner> initialOwner) {
   if (create && !initialOwner && !navigation->currentIndex().data(OrganizationTreeModel::EFFECTIVE_ARCHIVE_ROLE).toBool()) {
      if (const auto entity = selectedOrganization()) {
         const auto key = taskcompass::organizationKey(*entity);
         if (key.kind != taskcompass::OrganizationKind::PILLAR) {
            initialOwner = taskcompass::TaskOwner{key.kind == taskcompass::OrganizationKind::OBJECTIVE ? taskcompass::TaskOwnerKind::OBJECTIVE : taskcompass::TaskOwnerKind::PROJECT, key.id};
         }
      }
   }
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
   const auto snapshot = controller->workspace();
   auto owner = original ? original->owner : initialOwner.value_or(taskcompass::TaskOwner{});
   auto *ownerText = new QLineEdit(&dialog); ownerText->setReadOnly(true); ownerText->setObjectName("taskOwner");
   auto updateOwner = [&]() {
      if (owner.kind == taskcompass::TaskOwnerKind::INDEPENDENT) { ownerText->setText("Independent"); }
      else { ownerText->setText(organizationModel->pathFor({owner.kind == taskcompass::TaskOwnerKind::OBJECTIVE ? taskcompass::OrganizationKind::OBJECTIVE : taskcompass::OrganizationKind::PROJECT, owner.id})); }
   };
   auto *chooseOwner = new QPushButton("Choose Owner...", &dialog);
   connect(chooseOwner, &QPushButton::clicked, &dialog, [&]() {
      std::optional<taskcompass::OrganizationKey> parent;
      if (owner.kind != taskcompass::TaskOwnerKind::INDEPENDENT) {
         parent = {owner.kind == taskcompass::TaskOwnerKind::OBJECTIVE ? taskcompass::OrganizationKind::OBJECTIVE : taskcompass::OrganizationKind::PROJECT, owner.id};
      }
      OrganizationMoveDialog picker(snapshot, {}, parent, {}, !create, &dialog);
      if (picker.exec() == QDialog::Accepted) {
         const auto key = picker.parentChoice();
         owner = key ? taskcompass::TaskOwner{key->kind == taskcompass::OrganizationKind::OBJECTIVE ? taskcompass::TaskOwnerKind::OBJECTIVE : taskcompass::TaskOwnerKind::PROJECT, key->id} : taskcompass::TaskOwner{};
         updateOwner();
      }
   });
   updateOwner();
   form->addRow("&Title:", title);
   form->addRow("&Notes:", notes);
   form->addRow("Owner:", ownerText); form->addRow(chooseOwner);
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
   taskcompass::TaskCommand command{create ? taskcompass::TaskOperation::CREATE : taskcompass::TaskOperation::EDIT,
                                    original ? original->id : "", original ? original->localRevision : 0,
                                    title->text().toStdString(), value};
   if (create || owner != original->owner) {
      command.ownerChange = owner;
      if (owner.kind != taskcompass::TaskOwnerKind::INDEPENDENT || (!create && original->owner != owner)) {
         command.expectedWorkspace = taskcompass::workspaceRevisions(snapshot);
      }
      if (owner.kind != taskcompass::TaskOwnerKind::INDEPENDENT && !snapshot.organizations.empty()) {
         const taskcompass::OrganizationKey key{owner.kind == taskcompass::TaskOwnerKind::OBJECTIVE ? taskcompass::OrganizationKind::OBJECTIVE : taskcompass::OrganizationKind::PROJECT, owner.id};
         taskcompass::OrganizationRules graph(taskcompass::organizationFields(snapshot.organizations.front()).datasetId, snapshot.organizations);
         if (graph.effectivelyArchived(key)) {
            if (create || QMessageBox::question(this, "Archived destination", "The task will be hidden from active views until this owner is restored or Show Archived is enabled. Move it?") != QMessageBox::Yes) { return; }
            command.allowArchivedDestination = true;
         }
      }
   }
   controller->execute(command);
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

std::optional<taskcompass::OrganizationEntity> PersistentTaskWindow::selectedOrganization() const {
   const auto key = organizationModel->keyAt(navigation->currentIndex());
   if (!key) { return {}; }
   for (const auto &entity : controller->workspace().organizations) {
      if (taskcompass::organizationKey(entity) == *key && !taskcompass::organizationFields(entity).deletedAt) { return entity; }
   }
   return {};
}
QString PersistentTaskWindow::navigationSelection() const {
   const auto index = navigation->currentIndex();
   if (const auto key = organizationModel->keyAt(index)) { return QString::number(static_cast<int>(key->kind)) + ":" + QString::fromStdString(key->id); }
   return index.data(OrganizationTreeModel::SCOPE_ROLE).toInt() == 1 ? "independent" : "all";
}
void PersistentTaskWindow::rebuildNavigation() {
   if (!navigationRestored && !controller->isReady()) { return; }
   const QString selection = navigationRestored ? navigationSelection() : settings.value("navigationSelection", "all").toString();
   QStringList expanded = navigationRestored ? QStringList() : settings.value("expandedOrganization").toStringList();
   if (navigationRestored) {
      for (const auto &index : organizationModel->organizationIndexes()) {
         if (navigation->isExpanded(index)) { expanded.append(QString::number(index.data(OrganizationTreeModel::KIND_ROLE).toInt()) + ":" + index.data(OrganizationTreeModel::ID_ROLE).toString()); }
      }
   }
   const QSignalBlocker blocker(navigation->selectionModel());
   navigationRestored = true;
   organizationModel->replace(controller->workspace(), showArchived->isChecked(), navigationMode->currentIndex() == 1);
   organizationModel->setReparentingEnabled(controller->isReady() && !controller->isBusy() && navigationMode->currentIndex() == 0);
   QModelIndex selected = organizationModel->index(selection == "independent" ? 1 : 0, 0);
   for (const auto &index : organizationModel->organizationIndexes()) {
      const auto token = QString::number(index.data(OrganizationTreeModel::KIND_ROLE).toInt()) + ":" + index.data(OrganizationTreeModel::ID_ROLE).toString();
      if (expanded.contains(token)) { navigation->setExpanded(index, true); }
      if (token == selection) { selected = index; }
   }
   for (auto parent = selected.parent(); parent.isValid(); parent = parent.parent()) { navigation->setExpanded(parent, true); }
   navigation->setCurrentIndex(selected);
}
void PersistentTaskWindow::updateScope() {
   auto *filter = static_cast<PersistentTaskFilter *>(proxy);
   filter->setIncludeArchived(showArchived->isChecked());
   filter->setOrganizationScope(organizationModel->keyAt(navigation->currentIndex()), navigation->currentIndex().data(OrganizationTreeModel::SCOPE_ROLE).toInt() == 1);
}
void PersistentTaskWindow::editOrganization(std::optional<taskcompass::OrganizationKind> createKind, bool rootPillar) {
   const auto original = createKind ? std::optional<taskcompass::OrganizationEntity>() : selectedOrganization();
   if (!createKind && !original) { return; }
   const auto selected = selectedOrganization();
   std::optional<taskcompass::OrganizationKey> parent;
   if (selected) {
      const auto key = taskcompass::organizationKey(*selected);
      if (key.kind != taskcompass::OrganizationKind::OBJECTIVE && (!createKind || *createKind != taskcompass::OrganizationKind::PILLAR || key.kind == taskcompass::OrganizationKind::PILLAR)) { parent = key; }
   }
   if (rootPillar) { parent.reset(); }
   if (original) { parent = taskcompass::organizationParent(*original); }
   else if (navigation->currentIndex().data(OrganizationTreeModel::EFFECTIVE_ARCHIVE_ROLE).toBool()) { parent.reset(); }
   const auto kind = createKind ? *createKind : taskcompass::organizationKey(*original).kind;
   OrganizationEditDialog dialog(controller->workspace(), kind, original, parent, this);
   if (dialog.exec() == QDialog::Accepted) { controller->execute(dialog.command()); }
}
void PersistentTaskWindow::moveOrganization(std::optional<taskcompass::OrganizationKey> destination) {
   const auto entity = selectedOrganization();
   if (!entity) { return; }
   const auto snapshot = controller->workspace();
   const auto key = taskcompass::organizationKey(*entity);
   OrganizationMoveDialog dialog(snapshot, key.kind, destination ? destination : taskcompass::organizationParent(*entity), key, true, this);
   if (dialog.exec() != QDialog::Accepted) { return; }
   QString message = "Move this item and its subtree?\n" + subtreeImpact(snapshot, key);
   if (dialog.archivedDestination()) { message += "\nThe destination is archived. Moved work will be hidden unless Show Archived is enabled."; }
   if (QMessageBox::question(this, "Confirm move", message) != QMessageBox::Yes) { return; }
   taskcompass::OrganizationCommand command;
   command.operation = taskcompass::OrganizationOperation::MOVE; command.kind = key.kind; command.id = key.id;
   command.expectedRevision = taskcompass::organizationFields(*entity).localRevision;
   command.parent = dialog.parentChoice(); command.allowArchivedDestination = dialog.archivedDestination();
   command.expectedWorkspace = taskcompass::workspaceRevisions(snapshot);
   controller->execute(command);
}
void PersistentTaskWindow::archiveOrganization(bool restore) {
   const auto entity = selectedOrganization();
   if (!entity) { return; }
   const auto key = taskcompass::organizationKey(*entity);
   QString message = restore ? "Restore this item's explicit archive flag? Other archived items stay archived; an archived ancestor can still keep this item hidden."
                                  : "Archive this item? Its descendants and tasks will be hidden from active views. Use Show Archived to find and restore them.";
   message += "\n" + subtreeImpact(controller->workspace(), key);
   if (QMessageBox::question(this, restore ? "Restore organization" : "Archive organization", message) != QMessageBox::Yes) { return; }
   taskcompass::OrganizationCommand command;
   command.operation = restore ? taskcompass::OrganizationOperation::RESTORE : taskcompass::OrganizationOperation::ARCHIVE;
   command.kind = key.kind; command.id = key.id; command.expectedRevision = taskcompass::organizationFields(*entity).localRevision;
   command.expectedWorkspace = taskcompass::workspaceRevisions(controller->workspace());
   controller->execute(command);
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
   settings.setValue("objectiveView", navigationMode->currentIndex() == 1);
   settings.setValue("navigationSelection", navigationSelection());
   QStringList expanded;
   for (const auto &index : organizationModel->organizationIndexes()) {
      if (navigation->isExpanded(index)) { expanded.append(QString::number(index.data(OrganizationTreeModel::KIND_ROLE).toInt()) + ":" + index.data(OrganizationTreeModel::ID_ROLE).toString()); }
   }
   settings.setValue("expandedOrganization", expanded);
   settings.sync();
   QMainWindow::closeEvent(event);
}

void PersistentTaskWindow::populateCreationMenu(QMenu *menu) {
   menu->clear();
   menu->addAction(newAction);
   menu->addSeparator();
   const auto entity = selectedOrganization();
   const bool active = entity && !navigation->currentIndex().data(OrganizationTreeModel::EFFECTIVE_ARCHIVE_ROLE).toBool();
   if (active) {
      const auto kind = taskcompass::organizationKey(*entity).kind;
      auto add = [&](const QString &label, taskcompass::OrganizationKind createdKind) {
         auto *action = menu->addAction(label, this, [this, createdKind]() { editOrganization(createdKind); });
         action->setEnabled(newAction->isEnabled());
      };
      if (kind == taskcompass::OrganizationKind::PILLAR) { add("New Subpillar...", taskcompass::OrganizationKind::PILLAR); }
      if (kind != taskcompass::OrganizationKind::OBJECTIVE) {
         add(kind == taskcompass::OrganizationKind::PROJECT ? "New Subproject..." : "New Project...", taskcompass::OrganizationKind::PROJECT);
         add("New Objective...", taskcompass::OrganizationKind::OBJECTIVE);
      }
   }
   if (!menu->actions().back()->isSeparator()) { menu->addSeparator(); }
   auto *pillar = menu->addAction("New Pillar...", this, [this]() {
      // This action always means a root pillar; New Subpillar uses the selection.
      editOrganization(taskcompass::OrganizationKind::PILLAR, true);
   });
   pillar->setEnabled(newAction->isEnabled());
}

namespace {

std::optional<int> chooseContext(QWidget *parent, const QString &title, const QString &prompt,
                                 const std::vector<taskcompass::Context> &choices, bool allowUnassigned,
                                 int selected = 0) {
   QDialog dialog(parent);
   dialog.setWindowTitle(title);
   QFormLayout layout(&dialog);
   QComboBox labels(&dialog);
   labels.setObjectName("contextPicker");
   if (allowUnassigned) { labels.addItem("No context"); }
   QStringList captions;
   for (const auto &context : choices) {
      auto caption = QString::fromStdString(context.name);
      if (context.archivedAt) { caption += " [archived]"; }
      captions.push_back(caption);
   }
   for (int i = 0; i < captions.size(); ++i) {
      auto caption = captions.at(i);
      if (caption == "No context" || captions.count(caption) > 1) {
         caption += " (" + QString::fromStdString(choices.at(static_cast<std::size_t>(i)).id) + ")";
      }
      labels.addItem(caption);
   }
   // A literal context name can also match a generated disambiguation label.
   QStringList displayed;
   for (int i = 0; i < labels.count(); ++i) { displayed.push_back(labels.itemText(i)); }
   const int offset = allowUnassigned ? 1 : 0;
   for (int i = offset; i < labels.count(); ++i) {
      if (displayed.count(displayed.at(i)) > 1) {
         for (int row = offset; row < labels.count(); ++row) {
            labels.setItemText(row, captions.at(row - offset) + " (" +
               QString::fromStdString(choices.at(static_cast<std::size_t>(row - offset)).id) + ")");
         }
         break;
      }
   }
   labels.setCurrentIndex(selected);
   layout.addRow(prompt, &labels);
   QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
   layout.addRow(&buttons);
   QObject::connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
   QObject::connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
   if (dialog.exec() != QDialog::Accepted || labels.currentIndex() < 0) { return {}; }
   return labels.currentIndex();
}

}

void PersistentTaskWindow::changeContext(taskcompass::ContextOperation operation) {
   using Operation = taskcompass::ContextOperation;
   const auto snapshot = renderedWorkspace;
   taskcompass::ContextCommand command;
   command.operation = operation;
   if (operation == Operation::MOVE_UP || operation == Operation::MOVE_DOWN) {
      command.expectedWorkspace = taskcompass::workspaceRevisions(snapshot);
   }
   QString oldName;
   bool confirmed = false;
   if (operation != Operation::CREATE) {
      auto choices = taskcompass::orderedContexts(snapshot, true);
      if (operation == Operation::MOVE_UP && !choices.empty()) { choices.erase(choices.begin()); }
      if (operation == Operation::MOVE_DOWN && !choices.empty()) { choices.pop_back(); }
      std::erase_if(choices, [operation](const auto &context) {
         return (operation == Operation::RESTORE && !context.archivedAt)
                || (operation == Operation::ARCHIVE && context.archivedAt);
      });
      if (choices.empty()) {
         QMessageBox::information(this, "Contexts", "There are no contexts available for this action.");
         return;
      }
      const auto selected = chooseContext(this, "Contexts", "Context", choices, false);
      if (!selected) { return; }
      const auto &context = choices.at(static_cast<std::size_t>(*selected));
      command.id = context.id;
      command.expectedRevision = context.localRevision;
      oldName = QString::fromStdString(context.name);
      if (operation == Operation::DELETE_CONTEXT) {
         const taskcompass::ContextGuard guard{taskcompass::ContextGuardKind::LINKS, context.id, 0, {}, {}, {}, {}};
         command.expectedLinks.emplace();
         for (const auto &link : taskcompass::contextWitness(snapshot, guard)) {
            command.expectedLinks->push_back({taskcompass::entityKey(link), taskcompass::entityRevision(link)});
         }
         const auto count = command.expectedLinks->size();
         if (QMessageBox::question(this, "Delete Context",
                                   QString("Delete “%1” and clear %2 task assignment(s)? The tasks will remain.")
                                      .arg(oldName).arg(count)) != QMessageBox::Yes) { return; }
      }
   }
   if (operation == Operation::CREATE || operation == Operation::RENAME) {
      const auto name = QInputDialog::getText(this, "Context", "Name", QLineEdit::Normal, oldName, &confirmed);
      if (!confirmed || name.trimmed().isEmpty() || name == oldName) { return; }
      command.name = name.toStdString();
   }
   controller->execute(command);
}

void PersistentTaskWindow::editPriority() {
   const auto task = selectedTask();
   if (!task) { return; }
   QDialog dialog(this);
   dialog.setWindowTitle("Set Task Priority");
   QFormLayout layout(&dialog);
   QLineEdit input(QString::number(task->priority), &dialog);
   input.setObjectName("taskPriorityInput");
   input.selectAll();
   layout.addRow("&Priority (1 is highest)", &input);
   QLabel invalid("Enter a whole number from 1 to 9223372036854775807.", &dialog);
   layout.addRow(&invalid);
   QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
   layout.addRow(&buttons);
   const auto validate = [&]() {
      const auto value = taskcompass::parseTaskPriority(input.text().toStdString());
      invalid.setVisible(!value);
      buttons.button(QDialogButtonBox::Ok)->setEnabled(value && *value != task->priority);
   };
   connect(&input, &QLineEdit::textChanged, &dialog, validate);
   connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
   connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
   validate();
   if (dialog.exec() != QDialog::Accepted) { return; }
   const auto value = taskcompass::parseTaskPriority(input.text().toStdString());
   if (value && *value != task->priority) {
      controller->execute(taskcompass::TaskPriorityCommand{task->id, task->localRevision, *value});
   }
}

void PersistentTaskWindow::assignContext() {
   const auto task = selectedTask();
   if (!task) { return; }
   const auto snapshot = renderedWorkspace;
   const auto choices = taskcompass::orderedContexts(snapshot, false);
   const auto *link = taskcompass::primaryContext(snapshot, task->id);
   const auto *current = taskcompass::effectiveContext(snapshot, task->id, true);
   int selected = 0;
   if (link) {
      const auto assigned = std::ranges::find(choices, link->contextId, &taskcompass::Context::id);
      if (assigned != choices.end()) { selected = static_cast<int>(assigned - choices.begin()) + 1; }
   }
   const auto index = chooseContext(this, "Set Context", current && current->archivedAt
      ? QString("Context — currently assigned to archived “%1”").arg(QString::fromStdString(current->name)) : "Context",
      choices, true, selected);
   if (!index || (*index == 0 && !link)) { return; }
   taskcompass::ContextCommand command;
   command.operation = taskcompass::ContextOperation::ASSIGN;
   command.id = task->id;
   command.expectedRevision = task->localRevision;
   command.expectedWorkspace = taskcompass::workspaceRevisions(snapshot);
   if (*index > 0) {
      command.destination = choices.at(static_cast<std::size_t>(*index - 1)).id;
      if (link && link->contextId == *command.destination) { return; }
   }
   controller->execute(command);
}
