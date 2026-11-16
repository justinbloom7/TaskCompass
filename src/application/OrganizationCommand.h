#pragma once

#include "domain/OrganizationRules.h"

namespace taskcompass {

enum class EntityKind { TASK, PILLAR, PROJECT, OBJECTIVE };
struct EntityKey {
   EntityKind kind = EntityKind::TASK;
   std::string id;
   auto operator<=>(const EntityKey &) const = default;
};
struct RevisionCheck {
   EntityKey key;
   std::int64_t revision = 0;
   bool operator==(const RevisionCheck &) const = default;
};
enum class OrganizationOperation { CREATE, EDIT, MOVE, ARCHIVE, RESTORE };
struct OrganizationCommand {
   OrganizationOperation operation = OrganizationOperation::CREATE;
   OrganizationKind kind = OrganizationKind::PILLAR;
   std::string id;
   std::int64_t expectedRevision = 0;
   std::string title;
   std::optional<std::string> description;
   std::optional<OrganizationKey> parent;
   bool allowArchivedDestination = false;
   // Whole-preview preconditions reject added/changed records, not merely a
   // stale target row. Optional for noninteractive, transaction-local commands.
   std::optional<std::vector<RevisionCheck>> expectedWorkspace;
};

}
