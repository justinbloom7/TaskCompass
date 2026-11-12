#include "Task.h"

#include <algorithm>
#include <stdexcept>

namespace taskcompass {
namespace {
bool validUtf8(const std::string &value) {
   std::size_t index = 0;
   while (index < value.size()) {
      const auto first = static_cast<unsigned char>(value[index++]);
      if (first < 0x80) {
         continue;
      }
      int remaining = 0;
      std::uint32_t code = 0;
      std::uint32_t minimum = 0;
      if (first >= 0xc2 && first <= 0xdf) {
         remaining = 1; code = first & 0x1f; minimum = 0x80;
      } else if (first >= 0xe0 && first <= 0xef) {
         remaining = 2; code = first & 0x0f; minimum = 0x800;
      } else if (first >= 0xf0 && first <= 0xf4) {
         remaining = 3; code = first & 0x07; minimum = 0x10000;
      } else {
         return false;
      }
      for (int offset = 0; offset < remaining; ++offset) {
         if (index == value.size()) {
            return false;
         }
         const auto next = static_cast<unsigned char>(value[index++]);
         if ((next & 0xc0) != 0x80) {
            return false;
         }
         code = (code << 6) | (next & 0x3f);
      }
      if (code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) {
         return false;
      }
   }
   return true;
}
}

bool isUuid(const std::string &value) {
   if (value.size() != 36) {
      return false;
   }
   for (std::size_t index = 0; index < value.size(); ++index) {
      const char character = value[index];
      if (index == 8 || index == 13 || index == 18 || index == 23) {
         if (character != '-') {
            return false;
         }
      } else if (!(character >= '0' && character <= '9') && !(character >= 'a' && character <= 'f')) {
         return false;
      }
   }
   return true;
}

std::string normalizeTitle(const std::string &title) {
   constexpr auto WHITESPACE = " \t\r\n\f\v";
   const auto first = title.find_first_not_of(WHITESPACE);
   if (first == std::string::npos) {
      throw std::invalid_argument("A task title is required.");
   }
   const auto result = title.substr(first, title.find_last_not_of(WHITESPACE) - first + 1);
   // A byte limit keeps validation portable without a Unicode library in the core.
   if (!validUtf8(result) || result.size() > 1024 || result.find('\0') != std::string::npos) {
      throw std::invalid_argument("Title must contain valid UTF-8, at most 1024 bytes and no NUL characters.");
   }
   return result;
}

void validateTask(const Task &task) {
   if (!isUuid(task.id) || !isUuid(task.datasetId)) {
      throw std::invalid_argument("Invalid canonical task or dataset UUID.");
   }
   if (normalizeTitle(task.title) != task.title) {
      throw std::invalid_argument("Title must be trimmed.");
   }
   if (task.notes && (!validUtf8(*task.notes) || task.notes->size() > 65535 || task.notes->find('\0') != std::string::npos)) {
      throw std::invalid_argument("Notes must contain at most 65535 UTF-8 bytes and no NUL characters.");
   }
   if (task.status != TaskStatus::ACTIVE && task.status != TaskStatus::COMPLETED) {
      throw std::invalid_argument("Unsupported task status.");
   }
   if ((task.status == TaskStatus::COMPLETED) != task.completedAt.has_value()) {
      throw std::invalid_argument("Completion timestamp must agree with task status.");
   }
   if (task.createdAt <= 0 || task.updatedAt < task.createdAt || task.localRevision < 1
       || (task.completedAt && (*task.completedAt < task.createdAt || *task.completedAt > task.updatedAt))
       || (task.deletedAt && (*task.deletedAt < task.createdAt || *task.deletedAt > task.updatedAt))) {
      throw std::invalid_argument("Invalid task timestamps or revision.");
   }
}

}
