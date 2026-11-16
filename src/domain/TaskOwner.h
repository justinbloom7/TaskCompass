#pragma once

#include <string>

namespace taskcompass {

enum class TaskOwnerKind { INDEPENDENT, OBJECTIVE, PROJECT };

struct TaskOwner {
   TaskOwnerKind kind = TaskOwnerKind::INDEPENDENT;
   std::string id;
   bool operator==(const TaskOwner &) const = default;
};

void validateTaskOwner(const TaskOwner &owner);

}
