#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace taskcompass {

using Timestamp = std::int64_t; // UTC milliseconds since Unix epoch.

enum class TaskStatus { ACTIVE, COMPLETED };

struct Task {
   std::string id;
   std::string datasetId;
   std::string title;
   std::optional<std::string> notes;
   TaskStatus status = TaskStatus::ACTIVE;
   Timestamp createdAt = 0;
   Timestamp updatedAt = 0;
   std::optional<Timestamp> completedAt;
   std::optional<Timestamp> deletedAt;
   std::int64_t localRevision = 0;
   bool operator==(const Task &) const = default;
};

bool isUuid(const std::string &value);
std::string normalizeTitle(const std::string &title);
void validateTask(const Task &task);

}
