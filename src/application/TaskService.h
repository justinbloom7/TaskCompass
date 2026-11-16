#pragma once

#include "TaskRepository.h"

#include <functional>
#include <map>
#include <memory>

namespace taskcompass {

class TaskService {
public:
   using Clock = std::function<Timestamp()>;
   using IdGenerator = std::function<std::string()>;
   TaskService(TaskRepository &repository, std::string datasetId,
               Clock clock, IdGenerator generateId);
   ~TaskService();
   std::vector<Task> list();
   CommandReceipt execute(const TaskCommand &command);
   CommandReceipt undo();
   CommandReceipt redo();
   bool canUndo() const;
   bool canRedo() const;
   std::string undoDescription() const;
   std::string redoDescription() const;
   void clearHistory();

private:
   // Compatibility facade for accepted schema-1 tests/adapters. Production
   // organization sessions use CommandService directly, never a second stack.
   class Adapter;
   std::unique_ptr<Adapter> adapter;
   std::unique_ptr<class CommandService> commands;
};

}
