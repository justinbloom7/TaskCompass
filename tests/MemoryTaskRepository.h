#pragma once

#include "application/TaskRepository.h"
#include <map>
#include <stdexcept>

class MemoryTaskRepository : public taskcompass::TaskRepository {
public:
   std::map<std::string, taskcompass::Task> tasks;
   std::vector<taskcompass::CommandReceipt> receipts;
   bool failCommit = false;

   std::vector<taskcompass::Task> list() override {
      std::vector<taskcompass::Task> result;
      for (const auto &[id, task] : tasks) {
         (void)id;
         if (!task.deletedAt) {
            result.push_back(task);
         }
      }
      return result;
   }
   std::optional<taskcompass::Task> find(const std::string &id) override {
      return tasks.contains(id) ? std::optional(tasks.at(id)) : std::nullopt;
   }
   void commit(const taskcompass::CommandReceipt &receipt) override {
      if (failCommit) {
         throw std::runtime_error("Injected transaction failure");
      }
      const auto current = find(receipt.after.id);
      if (current.has_value() != receipt.before.has_value()
          || (current && current->localRevision != receipt.before->localRevision)) {
         throw std::runtime_error("Revision conflict");
      }
      tasks[receipt.after.id] = receipt.after;
      receipts.push_back(receipt);
   }
};
