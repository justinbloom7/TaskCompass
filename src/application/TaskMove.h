// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "OrganizationCommand.h"

namespace taskcompass {

struct WorkspaceSnapshot;

enum class TaskMovePosition { BEFORE, AFTER, GROUP_END };

struct TaskMoveTarget {
   std::string taskId;
   std::string contextId; // Empty identifies No context.
   TaskMovePosition position = TaskMovePosition::GROUP_END;
   std::string anchorId;
   bool operator==(const TaskMoveTarget &) const = default;
};

struct TaskMovePreview {
   TaskMoveTarget target;
   std::string datasetId;
   std::string sourceContextId;
   std::optional<std::string> clearedContextId;
   std::int64_t priority = 0;
   std::optional<RevisionCheck> visibleAnchor;
   std::optional<RevisionCheck> bandPredecessor;
   std::optional<RevisionCheck> bandSuccessor;
   std::vector<RevisionCheck> dependencies;
   std::size_t gap = 0;
   bool noChange = false;
   bool operator==(const TaskMovePreview &) const = default;
};

std::vector<Task> taskMoveRows(const WorkspaceSnapshot &snapshot, const std::string &contextId,
                              const std::string &excluding);
TaskMovePreview previewTaskMove(const WorkspaceSnapshot &snapshot, const TaskMoveTarget &target);

}
