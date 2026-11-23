// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "OrganizationCommand.h"

namespace taskcompass {

enum class ContextOperation { CREATE, RENAME, ARCHIVE, RESTORE, DELETE_CONTEXT, MOVE_UP, MOVE_DOWN, ASSIGN };

struct ContextCommand {
   ContextOperation operation = ContextOperation::CREATE;
   std::string id; // Context ID, or task ID for ASSIGN.
   std::int64_t expectedRevision = 0;
   std::string name;
   std::optional<std::string> destination;
   std::optional<std::vector<RevisionCheck>> expectedWorkspace;
   std::optional<std::vector<RevisionCheck>> expectedLinks;
};

}
