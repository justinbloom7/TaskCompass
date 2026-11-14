#include "TaskService.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace taskcompass {

TaskService::TaskService(TaskRepository &repository, std::string datasetId,
                         Clock clock, IdGenerator generateId)
   : repository(repository), datasetId(std::move(datasetId)), clock(std::move(clock)),
     generateId(std::move(generateId)) {
   if (!isUuid(this->datasetId)) {
      throw std::invalid_argument("Invalid dataset UUID.");
   }
}

std::vector<Task> TaskService::list() {
   return repository.list();
}

Task TaskService::requireTask(const std::string &id, std::int64_t revision) {
   const auto task = repository.find(id);
   if (!task || task->datasetId != datasetId || task->localRevision != revision) {
      throw std::runtime_error("Task changed or is unavailable. Refresh before retrying.");
   }
   validateTask(*task);
   if (task->updatedAt == std::numeric_limits<Timestamp>::max()) {
      throw std::runtime_error("Task timestamp exhausted.");
   }
   return *task;
}

CommandReceipt TaskService::makeReceipt(TaskOperation operation, const std::optional<Task> &before, Task after) {
   if (before && (before->localRevision == std::numeric_limits<std::int64_t>::max()
                  || before->updatedAt == std::numeric_limits<Timestamp>::max())) {
      throw std::runtime_error("Task revision or timestamp exhausted.");
   }
   after.localRevision = before ? before->localRevision + 1 : 1;
   after.updatedAt = before ? std::max(clock(), before->updatedAt + 1) : after.createdAt;
   after.updatedAt = std::max(after.updatedAt, after.completedAt.value_or(0));
   after.updatedAt = std::max(after.updatedAt, after.deletedAt.value_or(0));
   validateTask(after);
   const auto id = generateId();
   const auto operationId = generateId();
   if (!isUuid(id) || !isUuid(operationId)) {
      throw std::runtime_error("ID generator returned an invalid UUID.");
   }
   return {id, operationId, operation, before, std::move(after), {}, {}};
}

void TaskService::rememberRevision(const CommandReceipt &receipt) {
   knownRevisions[receipt.after.id] = receipt.after.localRevision;
}

CommandReceipt TaskService::execute(const TaskCommand &command) {
   std::optional<Task> before;
   Task after;
   if (command.operation == TaskOperation::CREATE) {
      after.id = generateId();
      after.datasetId = datasetId;
      after.createdAt = clock();
      after.title = normalizeTitle(command.title);
      after.notes = command.notes;
   } else {
      before = requireTask(command.taskId, command.expectedRevision);
      after = *before;
      if (after.deletedAt) {
         throw std::runtime_error("Task has been removed.");
      }
      switch (command.operation) {
         case TaskOperation::EDIT:
            after.title = normalizeTitle(command.title);
            after.notes = command.notes;
            break;
         case TaskOperation::COMPLETE:
            if (after.status != TaskStatus::ACTIVE) {
               throw std::invalid_argument("Only active tasks can be completed.");
            }
            after.status = TaskStatus::COMPLETED;
            after.completedAt = std::max(clock(), before->updatedAt + 1);
            break;
         case TaskOperation::REOPEN:
            if (after.status != TaskStatus::COMPLETED) {
               throw std::invalid_argument("Only completed tasks can be reopened.");
            }
            after.status = TaskStatus::ACTIVE;
            after.completedAt.reset();
            break;
         default: throw std::invalid_argument("Use the session history for undo/redo.");
      }
   }
   auto receipt = makeReceipt(command.operation, before, after);
   const bool externalRevision = before && knownRevisions.contains(before->id)
                                 && knownRevisions.at(before->id) != before->localRevision;
   repository.commit(receipt);
   if (externalRevision) {
      // A newly accepted edit must not make older stale inverses appear safe.
      clearHistory();
   }
   // Change history only after commit. A failed edit preserves the redo branch.
   history.erase(history.begin() + static_cast<std::ptrdiff_t>(historyIndex), history.end());
   history.push_back({receipt, receipt.id});
   if (history.size() > HISTORY_LIMIT) {
      history.erase(history.begin());
   }
   historyIndex = history.size();
   rememberRevision(receipt);
   return receipt;
}

CommandReceipt TaskService::compensate(bool forward) {
   if (forward ? !canRedo() : !canUndo()) {
      throw std::runtime_error("No command available in session history.");
   }
   auto &entry = history[forward ? historyIndex : historyIndex - 1];
   const auto &original = entry.original;
   const auto current = requireTask(original.after.id, knownRevisions.at(original.after.id));
   auto after = current;
   const auto &target = forward ? original.after : original.before.value_or(original.after);
   switch (original.operation) {
      case TaskOperation::CREATE:
         after.deletedAt = forward ? std::optional<Timestamp>() : std::optional<Timestamp>(std::max(clock(), current.updatedAt + 1));
         break;
      case TaskOperation::EDIT:
         after.title = target.title;
         after.notes = target.notes;
         break;
      case TaskOperation::COMPLETE:
      case TaskOperation::REOPEN:
         after.status = target.status;
         after.completedAt = target.completedAt;
         break;
      default: throw std::runtime_error("Unsupported history entry.");
   }
   auto receipt = makeReceipt(forward ? TaskOperation::REDO : TaskOperation::UNDO, current, after);
   if (forward) {
      receipt.reappliesCommandId = original.id;
   } else {
      receipt.compensatesCommandId = entry.lastCommandId;
   }
   repository.commit(receipt);
   entry.lastCommandId = receipt.id;
   if (forward) {
      ++historyIndex;
   } else {
      --historyIndex;
   }
   rememberRevision(receipt);
   return receipt;
}

CommandReceipt TaskService::undo() { return compensate(false); }
CommandReceipt TaskService::redo() { return compensate(true); }
bool TaskService::canUndo() const { return historyIndex > 0; }
bool TaskService::canRedo() const { return historyIndex < history.size(); }

namespace {
std::string description(TaskOperation operation) {
   switch (operation) {
      case TaskOperation::CREATE: return "Create task";
      case TaskOperation::EDIT: return "Edit task";
      case TaskOperation::COMPLETE: return "Complete task";
      case TaskOperation::REOPEN: return "Reopen task";
      default: return "";
   }
}
}
std::string TaskService::undoDescription() const {
   return canUndo() ? description(history[historyIndex - 1].original.operation) : "";
}
std::string TaskService::redoDescription() const {
   return canRedo() ? description(history[historyIndex].original.operation) : "";
}

void TaskService::clearHistory() {
   history.clear();
   knownRevisions.clear();
   historyIndex = 0;
}

}
