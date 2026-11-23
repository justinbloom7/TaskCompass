#include "PersistentTaskFilter.h"
#include "PersistentTaskModel.h"
#include <tuple>
#include <QMimeData>

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

namespace {
constexpr auto TASK_MOVE_MIME = "application/x-taskcompass-task-order";
}

void PersistentTaskFilter::setTaskMoveCallbacks(MovePreview preview, MoveCommit commit) {
   movePreview = std::move(preview);
   moveCommit = std::move(commit);
}

void PersistentTaskFilter::setTaskOrderingEnabled(bool enabled) { orderingEnabled = enabled; }

Qt::ItemFlags PersistentTaskFilter::flags(const QModelIndex &index) const {
   auto result = QSortFilterProxyModel::flags(index);
   if (orderingEnabled && index.isValid()) { result |= Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled; }
   return result;
}

QStringList PersistentTaskFilter::mimeTypes() const { return {TASK_MOVE_MIME}; }
Qt::DropActions PersistentTaskFilter::supportedDragActions() const { return Qt::MoveAction; }
Qt::DropActions PersistentTaskFilter::supportedDropActions() const { return Qt::MoveAction; }

const taskcompass::Task &PersistentTaskFilter::visibleTask(int row) const {
   return static_cast<const PersistentTaskModel *>(sourceModel())->taskAt(mapToSource(index(row, 0)).row());
}

std::string PersistentTaskFilter::visibleContext(int row) const {
   const auto *model = static_cast<const PersistentTaskModel *>(sourceModel());
   const auto *context = model->contextAt(mapToSource(index(row, 0)).row(), false);
   return context ? context->id : "";
}

QMimeData *PersistentTaskFilter::mimeData(const QModelIndexList &indexes) const {
   if (!orderingEnabled || indexes.size() != 1 || !indexes.front().isValid()) { return nullptr; }
   const auto &task = visibleTask(indexes.front().row());
   auto *data = new QMimeData;
   data->setData(TASK_MOVE_MIME, QByteArray::fromStdString(task.id) + '\n' + QByteArray::number(task.localRevision));
   return data;
}

std::optional<taskcompass::TaskMovePreview> PersistentTaskFilter::previewDrop(const QMimeData *data,
                                                                            taskcompass::TaskMoveTarget target) const {
   if (!orderingEnabled || !movePreview || !data || !data->hasFormat(TASK_MOVE_MIME)) { return {}; }
   const auto encoded = data->data(TASK_MOVE_MIME);
   if (encoded.size() < 38 || encoded.size() > 56 || encoded.at(36) != '\n') { return {}; }
   target.taskId = encoded.first(36).toStdString();
   if (!taskcompass::isUuid(target.taskId) || target.taskId == target.anchorId) { return {}; }
   bool valid = false;
   const auto value = encoded.sliced(37).toLongLong(&valid);
   if (!valid || value <= 0 || QByteArray::number(value) != encoded.sliced(37)) { return {}; }
   auto preview = movePreview(target);
   if (!preview || preview->dependencies.empty() || preview->dependencies.front().revision != value) { return {}; }
   return preview;
}

std::optional<taskcompass::TaskMovePreview> PersistentTaskFilter::previewTableDrop(const QMimeData *data,
                                                                                 const QModelIndex &hit,
                                                                                 bool after) const {
   using namespace taskcompass;
   if (!hit.isValid() || hit.model() != this || !data || !orderingEnabled) { return {}; }
   const auto group = visibleContext(hit.row());
   const auto source = data->data(TASK_MOVE_MIME).left(36).toStdString();
   if (visibleTask(hit.row()).id == source) { return {}; }
   if (!after) { return previewDrop(data, {{}, group, TaskMovePosition::BEFORE, visibleTask(hit.row()).id}); }
   for (int row = hit.row() + 1; row < rowCount() && visibleContext(row) == group; ++row) {
      const auto &id = visibleTask(row).id;
      if (id != source) { return previewDrop(data, {{}, group, TaskMovePosition::BEFORE, id}); }
   }
   return previewDrop(data, {{}, group, TaskMovePosition::AFTER, visibleTask(hit.row()).id});
}

std::optional<taskcompass::TaskMovePreview> PersistentTaskFilter::previewGroupDrop(const QMimeData *data,
                                                                                 const std::string &contextId) const {
   return previewDrop(data, {{}, contextId, taskcompass::TaskMovePosition::GROUP_END, {}});
}

bool PersistentTaskFilter::canDropMimeData(const QMimeData *data, Qt::DropAction action, int row, int,
                                           const QModelIndex &parent) const {
   return action == Qt::MoveAction
       && previewTableDrop(data, row >= 0 ? index(row, 0) : parent, row < 0).has_value();
}

bool PersistentTaskFilter::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int,
                                        const QModelIndex &parent) {
   if (action != Qt::MoveAction) { return false; }
   const auto preview = previewTableDrop(data, row >= 0 ? index(row, 0) : parent, row < 0);
   if (!preview) { return false; }
   commitTaskMove(*preview);
   return true;
}

void PersistentTaskFilter::commitTaskMove(const taskcompass::TaskMovePreview &preview) {
   if (orderingEnabled && moveCommit && !preview.noChange) { moveCommit(preview); }
}
