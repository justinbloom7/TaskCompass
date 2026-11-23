#include "PersistentTaskFilter.h"
#include "PersistentTaskModel.h"
#include <tuple>

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
   static_cast<PersistentTaskModel *>(sourceModel())->setArchivedContextsVisible(enabled);
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
   endFilterChange(QSortFilterProxyModel::Direction::Rows);
#else
   invalidateFilter();
#endif
}

bool PersistentTaskFilter::lessThan(const QModelIndex &left, const QModelIndex &right) const {
   const auto *model = static_cast<const PersistentTaskModel *>(sourceModel());
   if (left.column() == PersistentTaskModel::CONTEXT_COLUMN && model->hasContexts()) {
      const auto key = [&](int row) {
         const auto *context = model->contextAt(row, includeArchived);
         const auto &task = model->taskAt(row);
         return std::tuple{context == nullptr, context ? std::string_view(context->sortKey) : std::string_view{},
                           context ? std::string_view(context->id) : std::string_view{},
                           task.priority, std::string_view(task.manualOrderKey), std::string_view(task.id)};
      };
      return sortOrder() == Qt::AscendingOrder ? key(left.row()) < key(right.row()) : key(right.row()) < key(left.row());
   }
   if (left.column() == PersistentTaskModel::PRIORITY_COLUMN && model->hasContexts()) {
      const auto &a = model->taskAt(left.row());
      const auto &b = model->taskAt(right.row());
      return std::tie(a.priority, a.id) < std::tie(b.priority, b.id);
   }
   const int comparison = QString::compare(left.data().toString(), right.data().toString(), Qt::CaseInsensitive);
   return comparison == 0 ? left.data(Qt::UserRole).toString() < right.data(Qt::UserRole).toString() : comparison < 0;
}
