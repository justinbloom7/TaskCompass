#include "TaskService.h"
#include "CommandService.h"

#include <set>
#include <stdexcept>

namespace taskcompass {
namespace {
CommandReceipt taskReceipt(const WorkspaceReceipt &receipt) {
   TaskOperation operation;
   switch (receipt.operation) {
      case CommandOperation::CREATE: operation = TaskOperation::CREATE; break;
      case CommandOperation::EDIT: operation = TaskOperation::EDIT; break;
      case CommandOperation::COMPLETE: operation = TaskOperation::COMPLETE; break;
      case CommandOperation::REOPEN: operation = TaskOperation::REOPEN; break;
      case CommandOperation::UNDO: operation = TaskOperation::UNDO; break;
      case CommandOperation::REDO: operation = TaskOperation::REDO; break;
      default: throw std::invalid_argument("Task-only compatibility receipt expected.");
   }
   return {receipt.id, receipt.operationId, operation,
           receipt.before ? std::optional(std::get<Task>(*receipt.before)) : std::nullopt,
           std::get<Task>(receipt.after), receipt.compensatesCommandId, receipt.reappliesCommandId};
}
}
class TaskService::Adapter : public WorkspaceRepository {
public:
   explicit Adapter(TaskRepository &repository) : repository(repository) {}
   WorkspaceSnapshot read() override {
      auto tasks = repository.list();
      std::set<std::string> present;
      for (const auto &task : tasks) { present.insert(task.id); }
      for (const auto &id : seen) {
         if (!present.contains(id)) {
            if (const auto task = repository.find(id)) { tasks.push_back(*task); }
         }
      }
      return {tasks, {}};
   }
   WorkspaceReceipt transact(const Prepare &prepare) override {
      const auto receipt = prepare(read());
      repository.commit(taskReceipt(receipt));
      seen.insert(entityKey(receipt.after).id);
      return receipt;
   }
private:
   TaskRepository &repository;
   std::set<std::string> seen;
};
TaskService::TaskService(TaskRepository &repository, std::string datasetId,
                         Clock clock, IdGenerator generateId)
   : adapter(std::make_unique<Adapter>(repository)),
     commands(std::make_unique<CommandService>(*adapter, std::move(datasetId), std::move(clock), std::move(generateId))) {}
TaskService::~TaskService() = default;
std::vector<Task> TaskService::list() {
   auto tasks = commands->read().tasks;
   std::erase_if(tasks, [](const auto &task) { return task.deletedAt.has_value(); });
   return tasks;
}
CommandReceipt TaskService::execute(const TaskCommand &command) { return taskReceipt(commands->execute(command)); }
CommandReceipt TaskService::undo() { return taskReceipt(commands->undo()); }
CommandReceipt TaskService::redo() { return taskReceipt(commands->redo()); }
bool TaskService::canUndo() const { return commands->canUndo(); }
bool TaskService::canRedo() const { return commands->canRedo(); }
std::string TaskService::undoDescription() const { return commands->undoDescription(); }
std::string TaskService::redoDescription() const { return commands->redoDescription(); }
void TaskService::clearHistory() { commands->clearHistory(); }

}
