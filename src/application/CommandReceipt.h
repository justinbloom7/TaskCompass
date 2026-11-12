#pragma once

#include "TaskCommand.h"

namespace taskcompass {

struct CommandReceipt {
   std::string id;
   std::string operationId;
   TaskOperation operation = TaskOperation::CREATE;
   std::optional<Task> before;
   Task after;
   std::optional<std::string> compensatesCommandId;
   std::optional<std::string> reappliesCommandId;
};

}
