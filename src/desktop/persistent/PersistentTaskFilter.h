#pragma once

#include <QSortFilterProxyModel>
#include "domain/OrganizationRules.h"

class PersistentTaskFilter : public QSortFilterProxyModel {
public:
   void setIncludeCompleted(bool enabled);
   void setOrganizationScope(const std::optional<taskcompass::OrganizationKey> &scope, bool independentOnly);
   void setIncludeArchived(bool enabled);
protected:
   bool filterAcceptsRow(int row, const QModelIndex &parent) const override;
   bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
private:
   bool includeCompleted = false;
   bool includeArchived = false;
   bool independentOnly = false;
   std::optional<taskcompass::OrganizationKey> scope;
};
