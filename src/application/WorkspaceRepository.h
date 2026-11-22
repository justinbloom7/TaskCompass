#pragma once

#include "OrganizationCommand.h"
#include "CommitOutcomeUnknown.h"
#include <functional>

namespace taskcompass {

using EntitySnapshot = std::variant<Task, Pillar, Project, Objective>;
enum class CommandOperation { CREATE, EDIT, COMPLETE, REOPEN, MOVE, ARCHIVE, RESTORE, UNDO, REDO };
enum ChangedField : unsigned { TITLE = 1, NOTES = 2, OWNER = 4, ARCHIVE = 8, REMOVAL = 16, LIFECYCLE = 32 };
struct WorkspaceReceipt {
   std::string id;
   std::string operationId;
   CommandOperation operation = CommandOperation::CREATE;
   std::optional<EntitySnapshot> before;
   EntitySnapshot after;
   unsigned changedFields = 0;
   std::vector<RevisionCheck> dependencies;
   std::optional<std::string> compensatesCommandId;
   std::optional<std::string> reappliesCommandId;
};
struct WorkspaceSnapshot {
   std::vector<Task> tasks; // Includes tombstones for history/dependency validation.
   std::vector<OrganizationEntity> organizations;
   bool operator==(const WorkspaceSnapshot &) const = default;
};

EntityKey entityKey(const EntitySnapshot &entity);
OrganizationEntity asOrganization(const EntitySnapshot &entity);
EntitySnapshot asSnapshot(const OrganizationEntity &entity);
std::int64_t entityRevision(const EntitySnapshot &entity);
std::string entityDataset(const EntitySnapshot &entity);
std::vector<RevisionCheck> workspaceRevisions(const WorkspaceSnapshot &snapshot);

class WorkspaceRepository {
public:
   using Prepare = std::function<WorkspaceReceipt(const WorkspaceSnapshot &)>;
   virtual ~WorkspaceRepository() = default;
   virtual WorkspaceSnapshot read() = 0;
   // Adapter owns lease + transaction, reads fresh state, calls pure preparation,
   // CAS-writes target + receipt + outbox, then returns only after commit.
   virtual WorkspaceReceipt transact(const Prepare &prepare) = 0;
};

}
