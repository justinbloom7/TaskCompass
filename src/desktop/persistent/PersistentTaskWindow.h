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

class PersistentTaskWindow : public QMainWindow {
   Q_OBJECT
public:
   explicit PersistentTaskWindow(PersistentTaskController::SessionFactory factory, QWidget *parent = nullptr);

protected:
   void closeEvent(QCloseEvent *event) override;

private:
   void refresh();
   void editTask(bool create);
   void setCompleted(const QString &id, bool completed);
   std::optional<taskcompass::Task> selectedTask() const;
   QSettings settings;
   PersistentTaskController *controller;
   PersistentTaskModel *model;
   QSortFilterProxyModel *proxy;
   QTableView *table;
   QLineEdit *search;
   QCheckBox *showCompleted;
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
