#pragma once

#include "application/WorkspaceRepository.h"
#include <QSqlDatabase>

namespace taskcompass {

class MariaDbWorkspaceRepository : public WorkspaceRepository {
public:
   MariaDbWorkspaceRepository(QSqlDatabase database, std::string datasetId,
                             std::function<void(int)> afterWrite = {});
   WorkspaceSnapshot read() override;
   bool operationCommitted(const std::string &operationId, const std::string &expectedDataset) override;
   WorkspaceReceipt transact(const Prepare &prepare) override;
private:
   WorkspaceSnapshot load(int version);
   void writePatch(const WorkspacePatch &patch, const WorkspaceSnapshot &current);
   void write(const WorkspaceReceipt &receipt, const WorkspaceSnapshot &current);
   QSqlDatabase database;
   std::string datasetId;
   std::function<void(int)> afterWrite;
};

}
