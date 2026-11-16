#pragma once

#include <QSqlDatabase>
#include <QString>

namespace taskcompass {

class MutationLease {
public:
   explicit MutationLease(QSqlDatabase database, int timeoutSeconds = 2);
   ~MutationLease();
   MutationLease(const MutationLease &) = delete;
   MutationLease &operator=(const MutationLease &) = delete;
private:
   QSqlDatabase database;
   QString name;
};

}
