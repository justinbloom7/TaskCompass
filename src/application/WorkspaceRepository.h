#pragma once

#include "OrganizationCommand.h"
#include "domain/Context.h"
#include "CommitOutcomeUnknown.h"
#include <functional>

namespace taskcompass {

using EntitySnapshot = std::variant<Task, Pillar, Project, Objective, Context, TaskContext>;
enum class CommandOperation { CREATE, EDIT, COMPLETE, REOPEN, MOVE, ARCHIVE, RESTORE, UNDO, REDO, DELETE_CONTEXT, ASSIGN, REORDER };
enum ChangedField : unsigned { TITLE = 1, NOTES = 2, OWNER = 4, ARCHIVE = 8, REMOVAL = 16, LIFECYCLE = 32, RANK = 64 };
struct WorkspacePatch {
   std::optional<EntitySnapshot> before;
   EntitySnapshot after;
   unsigned changedFields;
};

enum class ContextGuardKind { ORDER, LINKS, BAND, TASK_LINK };
struct ContextGuard {
   ContextGuardKind kind;
   std::string id; // Context or task identity, or empty for No context.
   std::int64_t priority = 0;
   std::vector<EntitySnapshot> before;
   std::vector<EntitySnapshot> after;
   std::optional<EntityKey> left;
   std::optional<EntityKey> right;
};

struct AssignmentIntent {
   std::optional<std::string> sourceLinkId;
   std::optional<std::string> destinationLinkId;
   std::optional<std::string> sourceContextId;
   std::optional<std::string> destinationContextId;
   std::optional<std::string> effectiveSourceId;
   std::optional<std::string> effectiveDestinationId;
};

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
   std::vector<WorkspacePatch> patches;
   std::vector<ContextGuard> contextGuards;
   Timestamp issuedAt = 0;
   std::optional<AssignmentIntent> assignment = {};
};
struct WorkspaceSnapshot {
   std::vector<Task> tasks; // Includes tombstones for history/dependency validation.
   std::vector<OrganizationEntity> organizations;
   std::vector<Context> contexts = {};
   std::vector<TaskContext> taskContexts = {};
   int schemaVersion = 2;
   bool operator==(const WorkspaceSnapshot &) const = default;
};

EntityKey entityKey(const EntitySnapshot &entity);
OrganizationEntity asOrganization(const EntitySnapshot &entity);
EntitySnapshot asSnapshot(const OrganizationEntity &entity);
std::int64_t entityRevision(const EntitySnapshot &entity);
std::string entityDataset(const EntitySnapshot &entity);
std::vector<RevisionCheck> workspaceRevisions(const WorkspaceSnapshot &snapshot);

std::optional<EntitySnapshot> findEntity(const WorkspaceSnapshot &snapshot, const EntityKey &key);
void putEntity(WorkspaceSnapshot &snapshot, const EntitySnapshot &entity);
void validateContextState(const WorkspaceSnapshot &snapshot, const std::string &datasetId);
const TaskContext *primaryContext(const WorkspaceSnapshot &snapshot, const std::string &taskId);
const Context *effectiveContext(const WorkspaceSnapshot &snapshot, const std::string &taskId, bool includeArchived = false);
std::vector<Context> orderedContexts(const WorkspaceSnapshot &snapshot, bool includeArchived);
std::vector<EntitySnapshot> contextWitness(const WorkspaceSnapshot &snapshot, const ContextGuard &guard);

class WorkspaceRepository {
public:
   using Prepare = std::function<WorkspaceReceipt(const WorkspaceSnapshot &)>;
   virtual ~WorkspaceRepository() = default;
   virtual WorkspaceSnapshot read() = 0;
   virtual bool operationCommitted(const std::string &, const std::string &) {
      throw std::runtime_error("This repository cannot resolve an uncertain commit.");
   }
   // Adapter owns lease + transaction, reads fresh state, calls pure preparation,
   // CAS-writes target + receipt + outbox, then returns only after commit.
   virtual WorkspaceReceipt transact(const Prepare &prepare) = 0;
};

}
