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
   void execute(const taskcompass::TaskPriorityCommand &command);
   void execute(const taskcompass::OrganizationCommand &command);
   void execute(const taskcompass::ContextCommand &command);
   void execute(const taskcompass::TaskMovePreview &preview);
   void undo();
   void redo();
   bool isBusy() const;
   bool isReady() const;
   bool canUndo() const;
   bool canRedo() const;
   QString undoDescription() const;
   QString redoDescription() const;
   const std::vector<taskcompass::Task> &tasks() const;
   const taskcompass::WorkspaceSnapshot &workspace() const;

signals:
   void changed();
   void failed(const QString &message);
   void commitResolved(const QString &message);

private:
   void dispatch(std::function<void(PersistentTaskSession &)> operation);
   SessionFactory factory;
   QThread thread;
   QObject *worker;
   std::unique_ptr<PersistentTaskSession> session;
   std::string unresolvedDataset;
   std::string unresolvedOperation; // Worker thread only.
   std::vector<taskcompass::Task> currentTasks;
   taskcompass::WorkspaceSnapshot currentWorkspace;
   QString undoText;
   QString redoText;
   bool busy = false;
   bool ready = false;
   bool undoAvailable = false;
   bool redoAvailable = false;
};
