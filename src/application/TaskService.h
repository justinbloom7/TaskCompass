#pragma once

#include "TaskRepository.h"

#include <functional>
#include <map>

namespace taskcompass {

class TaskService {
public:
   using Clock = std::function<Timestamp()>;
   using IdGenerator = std::function<std::string()>;
   TaskService(TaskRepository &repository, std::string datasetId,
               Clock clock, IdGenerator generateId);
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
   struct HistoryEntry {
      CommandReceipt original;
      std::string lastCommandId;
   };
   CommandReceipt compensate(bool forward);
   CommandReceipt makeReceipt(TaskOperation operation, const std::optional<Task> &before, Task after);
   Task requireTask(const std::string &id, std::int64_t revision);
   void rememberRevision(const CommandReceipt &receipt);

   TaskRepository &repository;
   std::string datasetId;
   Clock clock;
   IdGenerator generateId;
   std::vector<HistoryEntry> history;
   std::size_t historyIndex = 0;
   std::map<std::string, std::int64_t> knownRevisions;
   static constexpr std::size_t HISTORY_LIMIT = 100;
};

}
