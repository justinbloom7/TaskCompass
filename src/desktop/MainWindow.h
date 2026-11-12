#pragma once

#include <QMainWindow>
#include <QSettings>

class QAction;
class QCloseEvent;
class QLabel;
class QLineEdit;
class QSplitter;
class QTableView;
class QTreeView;
class TaskFilterModel;
class TaskTableModel;

class MainWindow : public QMainWindow {
public:
   explicit MainWindow(QWidget *parent = nullptr);

protected:
   void closeEvent(QCloseEvent *event) override;

private:
   void createNavigation();
   void createActions();
   void newTask();
   void editTask();
   void selectSourceRow(int row);
   void updateStatus();

   QSettings settings;
   QSplitter *splitter;
   QTreeView *navigation;
   QTableView *table;
   QLineEdit *filterEdit;
   QLabel *contextLabel;
   TaskTableModel *taskModel;
   TaskFilterModel *filterModel;
   QAction *editAction;
   QAction *navigationAction;
   QString currentNavigationTag;
};
