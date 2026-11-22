#include "PersistentTaskFilter.h"
#include "PersistentTaskModel.h"

void PersistentTaskFilter::setIncludeCompleted(bool enabled) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
   beginFilterChange();
   includeCompleted = enabled;
   endFilterChange(QSortFilterProxyModel::Direction::Rows);
#else
   includeCompleted = enabled;
   invalidateFilter();
#endif
}

bool PersistentTaskFilter::filterAcceptsRow(int row, const QModelIndex &parent) const {
   const auto *tasks = static_cast<PersistentTaskModel *>(sourceModel());
   return (includeCompleted || tasks->taskAt(row).status == taskcompass::TaskStatus::ACTIVE)
          && (includeArchived || !tasks->effectivelyArchived(row))
          && tasks->matchesScope(row, scope, independentOnly)
          && QSortFilterProxyModel::filterAcceptsRow(row, parent);
}

void PersistentTaskFilter::setOrganizationScope(const std::optional<taskcompass::OrganizationKey> &replacement, bool independent) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
   beginFilterChange();
#endif
   scope = replacement; independentOnly = independent;
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
   endFilterChange(QSortFilterProxyModel::Direction::Rows);
#else
   invalidateFilter();
#endif
}
void PersistentTaskFilter::setIncludeArchived(bool enabled) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
   beginFilterChange();
#endif
   includeArchived = enabled;
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
   endFilterChange(QSortFilterProxyModel::Direction::Rows);
#else
   invalidateFilter();
#endif
}

bool PersistentTaskFilter::lessThan(const QModelIndex &left, const QModelIndex &right) const {
   const int comparison = QString::compare(left.data().toString(), right.data().toString(), Qt::CaseInsensitive);
   return comparison == 0 ? left.data(Qt::UserRole).toString() < right.data(Qt::UserRole).toString() : comparison < 0;
}
