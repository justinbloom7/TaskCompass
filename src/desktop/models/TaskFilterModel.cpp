#include "TaskFilterModel.h"
#include "TaskTableModel.h"

TaskFilterModel::TaskFilterModel(QObject *parent) : QSortFilterProxyModel(parent) {
   setFilterCaseSensitivity(Qt::CaseInsensitive);
   setSortCaseSensitivity(Qt::CaseInsensitive);
   setFilterKeyColumn(-1);
   setSortRole(TaskTableModel::SORT_ROLE);
   setDynamicSortFilter(true);
}

void TaskFilterModel::setNavigationTag(const QString &tag) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
   beginFilterChange();
   navigationTag = tag;
   endFilterChange(QSortFilterProxyModel::Direction::Rows);
#else
   navigationTag = tag;
   invalidateFilter();
#endif
}

bool TaskFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const {
   const QModelIndex sourceIndex = sourceModel()->index(sourceRow, 0, sourceParent);
   const QStringList tags = sourceIndex.data(TaskTableModel::NAVIGATION_TAGS_ROLE).toStringList();
   return (navigationTag.isEmpty() || tags.contains(navigationTag))
          && QSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent);
}
