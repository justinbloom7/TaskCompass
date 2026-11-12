#pragma once

#include "application/TaskRepository.h"

#include <QSqlDatabase>
#include <functional>

namespace taskcompass {

class MariaDbTaskRepository : public TaskRepository {
public:
   MariaDbTaskRepository(QSqlDatabase database, std::string datasetId,
                         std::function<void()> afterTaskWrite = {});
   std::vector<Task> list() override;
   std::optional<Task> find(const std::string &id) override;
   void commit(const CommandReceipt &receipt) override;

private:
   QSqlDatabase database;
   std::string datasetId;
   std::function<void()> afterTaskWrite; // Fault-injection seam; empty in application.
};

}
