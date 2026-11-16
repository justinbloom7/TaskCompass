#pragma once

#include "application/WorkspaceRepository.h"
#include <algorithm>
#include <stdexcept>

class MemoryWorkspaceRepository : public taskcompass::WorkspaceRepository {
public:
   taskcompass::WorkspaceSnapshot snapshot;
   std::vector<taskcompass::WorkspaceReceipt> receipts;
   std::vector<taskcompass::WorkspaceReceipt> changes;
   int failAfter = -1;
   bool uncertain = false;

   taskcompass::WorkspaceSnapshot read() override { return snapshot; }
   taskcompass::WorkspaceReceipt transact(const Prepare &prepare) override {
      using namespace taskcompass;
      const auto receipt = prepare(snapshot);
      auto next = snapshot;
      auto replace = [&](auto &records, const auto &value, auto keyOf) {
         auto found = std::find_if(records.begin(), records.end(), [&](const auto &record) { return keyOf(record) == entityKey(receipt.after); });
         auto revisionOf = [](const auto &record) {
            if constexpr (std::is_same_v<std::decay_t<decltype(record)>, Task>) { return record.localRevision; }
            else { return organizationFields(record).localRevision; }
         };
         if ((found != records.end()) != receipt.before.has_value()
             || (found != records.end() && (revisionOf(*found) != entityRevision(*receipt.before)
                 || entityRevision(receipt.after) != entityRevision(*receipt.before) + 1))) {
            throw std::runtime_error("Fake revision transition failed");
         }
         if (found == records.end()) { records.push_back(value); } else { *found = value; }
      };
      if (const auto *task = std::get_if<Task>(&receipt.after)) {
         replace(next.tasks, *task, [](const auto &record) { return entityKey(record); });
      } else {
         replace(next.organizations, asOrganization(receipt.after), [](const auto &record) { return entityKey(asSnapshot(record)); });
      }
      if (failAfter >= 0) { throw std::runtime_error("Injected rolled-back workspace transaction"); }
      snapshot = std::move(next);
      receipts.push_back(receipt);
      changes.push_back(receipt);
      if (uncertain) { throw CommitOutcomeUnknown("Injected uncertain commit"); }
      return receipt;
   }
};
