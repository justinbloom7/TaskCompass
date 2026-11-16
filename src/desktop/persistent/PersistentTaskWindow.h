#pragma once

#include "PersistentTaskController.h"
#include <QMainWindow>
#include <QSettings>

class PersistentTaskModel;
class QSortFilterProxyModel;
class QTableView;
class QLineEdit;
class QCheckBox;
class QSplitter;
class QTreeView;
class QAction;
class QComboBox;
class OrganizationTreeModel;

class PersistentTaskWindow : public QMainWindow {
   Q_OBJECT
public:
   explicit PersistentTaskWindow(PersistentTaskController::SessionFactory factory, QWidget *parent = nullptr);

protected:
   void closeEvent(QCloseEvent *event) override;

private:
   void refresh();
   void editTask(bool create, std::optional<taskcompass::TaskOwner> initialOwner = {});
   void editOrganization(std::optional<taskcompass::OrganizationKind> createKind);
   void moveOrganization();
   void archiveOrganization(bool restore);
   void rebuildNavigation();
   void updateScope();
   QString navigationSelection() const;
   std::optional<taskcompass::OrganizationEntity> selectedOrganization() const;
   void setCompleted(const QString &id, bool completed);
   std::optional<taskcompass::Task> selectedTask() const;
   QSettings settings;
   PersistentTaskController *controller;
   PersistentTaskModel *model;
   QSortFilterProxyModel *proxy;
   QTableView *table;
   QLineEdit *search;
   QCheckBox *showCompleted;
   QCheckBox *showArchived;
   QComboBox *navigationMode;
   OrganizationTreeModel *organizationModel;
   taskcompass::WorkspaceSnapshot renderedWorkspace;
   bool navigationRestored = false;
   std::vector<QAction *> organizationActions;
   QSplitter *splitter;
   QTreeView *navigation;
   QAction *newAction;
   QAction *editAction;
   QAction *completeAction;
   QAction *undoAction;
   QAction *redoAction;
   QAction *reloadAction;
   QAction *navigationAction;
};
