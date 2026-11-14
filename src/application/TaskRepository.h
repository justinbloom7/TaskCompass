#pragma once

#include "CommandReceipt.h"
#include "CommitOutcomeUnknown.h"

#include <vector>

namespace taskcompass {

class TaskRepository {
public:
   virtual ~TaskRepository() = default;
   virtual std::vector<Task> list() = 0; // Includes completed; excludes tombstones.
   virtual std::optional<Task> find(const std::string &id) = 0; // Includes tombstones.
   // Atomically compare revision and persist task, receipt and dormant change.
   // A missing before snapshot means insert; no physical task deletes.
   virtual void commit(const CommandReceipt &receipt) = 0;
};

}
