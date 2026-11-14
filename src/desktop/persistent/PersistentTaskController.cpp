#include "PersistentTaskController.h"

#include <QPointer>
#include <exception>
#include <utility>

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

void PersistentTaskController::dispatch(std::function<void(taskcompass::TaskService &)> operation) {
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
         operation(*session->service);
         auto result = session->service->list();
         const bool undo = session->service->canUndo();
         const bool redo = session->service->canRedo();
         const auto undoLabel = QString::fromStdString(session->service->undoDescription());
         const auto redoLabel = QString::fromStdString(session->service->redoDescription());
         QMetaObject::invokeMethod(this, [receiver, undoLabel, redoLabel, result = std::move(result), undo, redo]() {
            if (!receiver) {
               return;
            }
            receiver->currentTasks = result;
            receiver->undoText = undoLabel;
            receiver->redoText = redoLabel;
            receiver->undoAvailable = undo;
            receiver->redoAvailable = redo;
            receiver->busy = false;
            receiver->ready = true;
            emit receiver->changed();
         });
      } catch (const std::exception &error) {
         if (dynamic_cast<const taskcompass::CommitOutcomeUnknown *>(&error) && session) {
            session->service->clearHistory();
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
   dispatch([](taskcompass::TaskService &) {});
}
void PersistentTaskController::execute(const taskcompass::TaskCommand &command) {
   if (ready) {
      dispatch([command](taskcompass::TaskService &service) { service.execute(command); });
   }
}
void PersistentTaskController::undo() {
   if (ready) {
      dispatch([](taskcompass::TaskService &service) { service.undo(); });
   }
}
void PersistentTaskController::redo() {
   if (ready) {
      dispatch([](taskcompass::TaskService &service) { service.redo(); });
   }
}
bool PersistentTaskController::isBusy() const { return busy; }
bool PersistentTaskController::isReady() const { return ready; }
bool PersistentTaskController::canUndo() const { return ready && !busy && undoAvailable; }
bool PersistentTaskController::canRedo() const { return ready && !busy && redoAvailable; }
const std::vector<taskcompass::Task> &PersistentTaskController::tasks() const { return currentTasks; }

QString PersistentTaskController::undoDescription() const { return undoText; }
QString PersistentTaskController::redoDescription() const { return redoText; }
