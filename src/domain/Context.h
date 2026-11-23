// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Task.h"
#include <vector>

namespace taskcompass {

struct Context {
   std::string id;
   std::string datasetId;
   std::string name;
   std::string sortKey;
   Timestamp createdAt = 0;
   Timestamp updatedAt = 0;
   std::optional<Timestamp> archivedAt;
   std::optional<Timestamp> deletedAt;
   std::int64_t localRevision = 0;
   bool operator==(const Context &) const = default;
};

struct TaskContext {
   std::string id;
   std::string datasetId;
   std::string taskId;
   std::string contextId;
   Timestamp createdAt = 0;
   Timestamp updatedAt = 0;
   std::optional<Timestamp> deletedAt;
   std::int64_t localRevision = 0;
   bool operator==(const TaskContext &) const = default;
};

void validateContext(const Context &context);
void validateTaskContext(const TaskContext &link);

}
