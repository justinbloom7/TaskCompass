#include "PersistentTaskController.h"

#include <QPointer>
#include <exception>
#include <utility>
#include <algorithm>

PersistentTaskController::PersistentTaskController(SessionFactory factory, QObject *parent)
   : QObject(parent), factory(std::move(factory)), worker(new QObject) {
   worker->moveToThread(&thread);
   connect(&thread, &QThread::finished, worker, &QObject::deleteLater);
   thread.start();
}

PersistentTaskController::~PersistentTaskController() {
   // Drain queued work and release every query/connection on its owning thread.
   QMetaObject::invokeMethod(worker, [this]() { session.reset(); }, Qt::BlockingQueuedConnection);
   thread.quit();
   thread.wait();
}

void PersistentTaskController::dispatch(std::function<void(PersistentTaskSession &)> operation) {
   if (busy) {
      return;
   }
   busy = true;
   emit changed();
   const QPointer<PersistentTaskController> receiver(this);
   QMetaObject::invokeMethod(worker, [this, receiver, operation = std::move(operation)]() {
      try {
         if (!session) {
            session = factory();
         }
         QString resolution;
         if (!unresolvedOperation.empty()) {
            const bool committed = session->workspaceRepository->operationCommitted(unresolvedOperation, unresolvedDataset);
            resolution = committed ? "The previous operation committed. Workspace reloaded."
                                   : "The previous operation was absent from the audit and outbox. Workspace reloaded.";
         }
         operation(*session);
         auto workspace = session->commands ? session->commands->read() : taskcompass::WorkspaceSnapshot{session->service->list(), {}};
         auto result = workspace.tasks;
         std::erase_if(result, [](const auto &task) { return task.deletedAt.has_value(); });
         const bool undo = session->commands ? session->commands->canUndo() : session->service->canUndo();
         const bool redo = session->commands ? session->commands->canRedo() : session->service->canRedo();
         const auto undoLabel = QString::fromStdString(session->commands ? session->commands->undoDescription() : session->service->undoDescription());
         const auto redoLabel = QString::fromStdString(session->commands ? session->commands->redoDescription() : session->service->redoDescription());
         unresolvedOperation.clear();
         unresolvedDataset.clear();
         QMetaObject::invokeMethod(this, [receiver, undoLabel, redoLabel, resolution, result = std::move(result), workspace = std::move(workspace), undo, redo]() {
            if (!receiver) {
               return;
            }
            receiver->currentTasks = result;
            receiver->currentWorkspace = workspace;
            receiver->undoText = undoLabel;
            receiver->redoText = redoLabel;
            receiver->undoAvailable = undo;
            receiver->redoAvailable = redo;
            receiver->busy = false;
            receiver->ready = true;
            emit receiver->changed();
            if (!resolution.isEmpty()) { emit receiver->commitResolved(resolution); }
         });
      } catch (const std::exception &error) {
         if (const auto *unknown = dynamic_cast<const taskcompass::CommitOutcomeUnknown *>(&error); unknown && session) {
            if (session->commands) { session->commands->clearHistory(); }
            else { session->service->clearHistory(); }
            if (!unknown->operationId.empty()) {
               unresolvedOperation = unknown->operationId;
               unresolvedDataset = unknown->datasetId;
               session.reset();
            }
         }
         const auto message = QString::fromUtf8(error.what());
         // Disable editing until explicit reload, preserving the last visible snapshot.
         QMetaObject::invokeMethod(this, [receiver, message]() {
            if (!receiver) {
               return;
            }
            receiver->busy = false;
            receiver->ready = false;
            emit receiver->changed();
            emit receiver->failed(message);
         });
      }
   });
}

void PersistentTaskController::reload() {
   dispatch([](PersistentTaskSession &) {});
}
void PersistentTaskController::execute(const taskcompass::TaskCommand &command) {
   if (ready) {
      dispatch([command](PersistentTaskSession &session) {
         if (session.commands) { session.commands->execute(command); }
         else { session.service->execute(command); }
      });
   }
}
void PersistentTaskController::execute(const taskcompass::OrganizationCommand &command) {
   if (ready) {
      dispatch([command](PersistentTaskSession &session) {
         if (!session.commands) { throw std::runtime_error("Organization requires a schema-2 workspace."); }
         session.commands->execute(command);
      });
   }
}
void PersistentTaskController::execute(const taskcompass::ContextCommand &command) {
   if (ready) {
      dispatch([command](PersistentTaskSession &session) {
         if (!session.commands) { throw std::runtime_error("Contexts require a schema-3 workspace."); }
         session.commands->execute(command);
      });
   }
}
void PersistentTaskController::undo() {
   if (ready) {
      dispatch([](PersistentTaskSession &session) { if (session.commands) { session.commands->undo(); } else { session.service->undo(); } });
   }
}
void PersistentTaskController::redo() {
   if (ready) {
      dispatch([](PersistentTaskSession &session) { if (session.commands) { session.commands->redo(); } else { session.service->redo(); } });
   }
}
bool PersistentTaskController::isBusy() const { return busy; }
bool PersistentTaskController::isReady() const { return ready; }
bool PersistentTaskController::canUndo() const { return ready && !busy && undoAvailable; }
bool PersistentTaskController::canRedo() const { return ready && !busy && redoAvailable; }
const std::vector<taskcompass::Task> &PersistentTaskController::tasks() const { return currentTasks; }
const taskcompass::WorkspaceSnapshot &PersistentTaskController::workspace() const { return currentWorkspace; }

QString PersistentTaskController::undoDescription() const { return undoText; }
QString PersistentTaskController::redoDescription() const { return redoText; }
