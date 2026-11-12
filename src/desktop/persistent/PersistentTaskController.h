#pragma once

#include "PersistentTaskSession.h"

#include <QObject>
#include <QThread>
#include <functional>
#include <memory>

class PersistentTaskController : public QObject {
   Q_OBJECT
public:
   using SessionFactory = std::function<std::unique_ptr<PersistentTaskSession>()>;
   explicit PersistentTaskController(SessionFactory factory, QObject *parent = nullptr);
   ~PersistentTaskController() override;
   void reload();
   void execute(const taskcompass::TaskCommand &command);
   void undo();
   void redo();
   bool isBusy() const;
   bool isReady() const;
   bool canUndo() const;
   bool canRedo() const;
   QString undoDescription() const;
   QString redoDescription() const;
   const std::vector<taskcompass::Task> &tasks() const;

signals:
   void changed();
   void failed(const QString &message);

private:
   void dispatch(std::function<void(taskcompass::TaskService &)> operation);
   SessionFactory factory;
   QThread thread;
   QObject *worker;
   std::unique_ptr<PersistentTaskSession> session;
   std::vector<taskcompass::Task> currentTasks;
   QString undoText;
   QString redoText;
   bool busy = false;
   bool ready = false;
   bool undoAvailable = false;
   bool redoAvailable = false;
};
