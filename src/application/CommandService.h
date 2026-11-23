#pragma once

#include "WorkspaceRepository.h"
#include "TaskCommand.h"
#include "ContextCommand.h"
#include <map>

namespace taskcompass {

class CommandService {
public:
   using Clock = std::function<Timestamp()>;
   using IdGenerator = std::function<std::string()>;
   CommandService(WorkspaceRepository &repository, std::string datasetId,
                  Clock clock, IdGenerator generateId);
   WorkspaceSnapshot read();
   WorkspaceReceipt execute(const TaskCommand &command);
   WorkspaceReceipt execute(const TaskPriorityCommand &command);
   WorkspaceReceipt execute(const OrganizationCommand &command);
   WorkspaceReceipt execute(const ContextCommand &command);
   WorkspaceReceipt undo();
   WorkspaceReceipt redo();
   bool canUndo() const;
   bool canRedo() const;
   std::string undoDescription() const;
   std::string redoDescription() const;
   void clearHistory();

private:
   struct HistoryEntry {
      WorkspaceReceipt original;
      std::string lastCommandId;
   };
   void prepareTaskCaptureRank(WorkspaceReceipt &receipt, const WorkspaceSnapshot &snapshot);
   void remember(const WorkspaceReceipt &receipt);
   WorkspaceReceipt compensateContext(const WorkspaceReceipt &original, const WorkspaceSnapshot &snapshot, bool forward, const std::string &lastCommandId);
   WorkspaceReceipt executePrepared(const WorkspaceRepository::Prepare &prepare);
   WorkspaceReceipt compensate(bool forward);
   WorkspaceReceipt receipt(CommandOperation operation, const std::optional<EntitySnapshot> &before,
                            EntitySnapshot after, unsigned changedFields, const WorkspaceSnapshot &snapshot);
   EntitySnapshot requireEntity(const WorkspaceSnapshot &snapshot, const EntityKey &key, std::int64_t revision) const;
   void validateChange(const WorkspaceSnapshot &snapshot, const EntitySnapshot &after, bool allowArchived) const;
   WorkspaceRepository &repository;
   std::string datasetId;
   Clock clock;
   IdGenerator generateId;
   std::vector<HistoryEntry> history;
   std::size_t historyIndex = 0;
   std::map<EntityKey, std::int64_t> knownRevisions;
   static constexpr std::size_t HISTORY_LIMIT = 100;
};

}
