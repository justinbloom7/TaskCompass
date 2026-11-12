#pragma once

#include "domain/Task.h"

namespace taskcompass {

enum class TaskOperation { CREATE, EDIT, COMPLETE, REOPEN, UNDO, REDO };

struct TaskCommand {
   TaskOperation operation = TaskOperation::CREATE;
   std::string taskId;
   std::int64_t expectedRevision = 0;
   std::string title;
   std::optional<std::string> notes;
};

}
