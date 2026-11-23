// SPDX-License-Identifier: GPL-3.0-or-later
#include "Context.h"
#include "FractionalRank.h"
#include <stdexcept>

namespace taskcompass {
namespace {

template<typename Record>
void validateFields(const Record &record) {
   if (!isUuid(record.id) || !isUuid(record.datasetId) || record.createdAt <= 0
       || record.updatedAt < record.createdAt || record.localRevision <= 0
       || (record.deletedAt && (*record.deletedAt < record.createdAt || *record.deletedAt > record.updatedAt))) {
      throw std::invalid_argument("Invalid context record identity, times or revision.");
   }
}

}

void validateContext(const Context &context) {
   validateFields(context);
   if (normalizeTitle(context.name) != context.name || !validRank(context.sortKey)
       || (context.archivedAt && (*context.archivedAt < context.createdAt || *context.archivedAt > context.updatedAt))) {
      throw std::invalid_argument("Invalid context name, rank or archive state.");
   }
}

void validateTaskContext(const TaskContext &link) {
   validateFields(link);
   if (!isUuid(link.taskId) || !isUuid(link.contextId)) { throw std::invalid_argument("Invalid task context link."); }
}

}
