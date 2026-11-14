#pragma once

#include <QSortFilterProxyModel>

class PersistentTaskFilter : public QSortFilterProxyModel {
public:
   void setIncludeCompleted(bool enabled);
protected:
   bool filterAcceptsRow(int row, const QModelIndex &parent) const override;
   bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
private:
   bool includeCompleted = false;
};
