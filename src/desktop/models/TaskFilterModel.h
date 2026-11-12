#pragma once

#include <QSortFilterProxyModel>

class TaskFilterModel : public QSortFilterProxyModel {
public:
   explicit TaskFilterModel(QObject *parent = nullptr);
   void setNavigationTag(const QString &tag);

protected:
   bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
   QString navigationTag;
};
