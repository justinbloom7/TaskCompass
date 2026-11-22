#pragma once

#include "application/WorkspaceRepository.h"
#include <QSqlDatabase>

namespace taskcompass {

class MariaDbWorkspaceRepository : public WorkspaceRepository {
public:
   MariaDbWorkspaceRepository(QSqlDatabase database, std::string datasetId,
                             std::function<void(int)> afterWrite = {});
   WorkspaceSnapshot read() override;
   WorkspaceReceipt transact(const Prepare &prepare) override;
private:
   WorkspaceSnapshot load();
   void write(const WorkspaceReceipt &receipt, const WorkspaceSnapshot &current);
   QSqlDatabase database;
   std::string datasetId;
   std::function<void(int)> afterWrite;
};

}
