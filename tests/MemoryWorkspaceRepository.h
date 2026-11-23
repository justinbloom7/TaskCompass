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
      const auto apply = [&](const WorkspacePatch &patch) {
         const auto prior = findEntity(next, entityKey(patch.after));
         if (prior != patch.before || entityRevision(patch.after) != (prior ? entityRevision(*prior) + 1 : 1)) {
            throw std::runtime_error("Fake revision transition failed");
         }
         putEntity(next, patch.after);
      };
      apply({receipt.before, receipt.after, receipt.changedFields});
      for (std::size_t index = 0; index < receipt.patches.size(); ++index) {
         apply(receipt.patches[index]);
         if (failAfter == static_cast<int>(index + 1)) { throw std::runtime_error("Injected participant failure"); }
      }
      validateContextState(next, entityDataset(receipt.after));
      if (failAfter >= 0) { throw std::runtime_error("Injected rolled-back workspace transaction"); }
      snapshot = std::move(next);
      receipts.push_back(receipt);
      changes.push_back(receipt);
      if (uncertain) { throw CommitOutcomeUnknown("Injected uncertain commit"); }
      return receipt;
   }
};
