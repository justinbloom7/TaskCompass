#include "PersistentTaskModel.h"

#include <QDateTime>
#include <QTimeZone>
#include <QStringList>
#include <algorithm>

PersistentTaskModel::PersistentTaskModel(QObject *parent) : QAbstractTableModel(parent) {}
int PersistentTaskModel::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : static_cast<int>(tasks.size()); }
int PersistentTaskModel::columnCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : COLUMN_COUNT; }

QVariant PersistentTaskModel::data(const QModelIndex &index, int role) const {
   if (!index.isValid() || index.row() < 0 || index.row() >= rowCount() || index.column() < 0 || index.column() >= COLUMN_COUNT) {
      return {};
   }
   const auto &task = tasks.at(index.row());
   if (role == Qt::UserRole) {
      return QString::fromStdString(task.id);
   }
   if (role == Qt::ToolTipRole) {
      if (index.column() == CONTEXT_COLUMN) {
         const auto *assigned = contextAt(index.row(), true);
         if (assigned && assigned->archivedAt) { return "Archived assignment: " + QString::fromStdString(assigned->name); }
      }
      return QString::fromStdString(task.notes.value_or(""));
   }
   if (role == Qt::CheckStateRole && index.column() == STATUS_COLUMN) {
      return task.status == taskcompass::TaskStatus::COMPLETED ? Qt::Checked : Qt::Unchecked;
   }
   if (role != Qt::DisplayRole) {
      return {};
   }
   auto date = [](taskcompass::Timestamp value) {
      return QDateTime::fromMSecsSinceEpoch(value, QTimeZone("UTC")).toLocalTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
   };
   switch (index.column()) {
      case TITLE_COLUMN: return QString::fromStdString(task.title);
      case STATUS_COLUMN: return task.status == taskcompass::TaskStatus::ACTIVE ? "Active" : "Completed";
      case CREATED_COLUMN: return date(task.createdAt);
      case UPDATED_COLUMN: return date(task.updatedAt);
      case COMPLETED_COLUMN: return task.completedAt ? date(*task.completedAt) : QString();
      case PRIORITY_COLUMN: return hasContexts() ? QVariant::fromValue<qlonglong>(task.priority) : QVariant();
      case CONTEXT_COLUMN: {
         const auto *context = contextAt(index.row(), archivedContextsVisible);
         if (!context) { return "No context"; }
         auto label = QString::fromStdString(context->name);
         if (context->archivedAt) { label += " [archived]"; }
         if (context->name == "No context" || std::ranges::count_if(contextSnapshot.contexts, [&](const auto &item) {
            return !item.deletedAt && item.name == context->name;
         }) > 1) { label += " (" + QString::fromStdString(context->id) + ")"; }
         return label;
      }
      case ORGANIZATION_COLUMN: {
         if (!graph || task.owner.kind == taskcompass::TaskOwnerKind::INDEPENDENT) { return "Independent"; }
         const taskcompass::OrganizationKey owner{task.owner.kind == taskcompass::TaskOwnerKind::OBJECTIVE ? taskcompass::OrganizationKind::OBJECTIVE : taskcompass::OrganizationKind::PROJECT, task.owner.id};
         QStringList parts;
         for (const auto &key : graph->path(owner)) { parts.append(QString::fromStdString(taskcompass::organizationTitle(graph->find(key)))); }
         return parts.join(" / ") + (graph->taskArchived(task) ? " [archived context]" : "");
      }
      default: return {};
   }
}

QVariant PersistentTaskModel::headerData(int section, Qt::Orientation orientation, int role) const {
   if (role != Qt::DisplayRole) {
      return {};
   }
   if (orientation == Qt::Vertical) {
      return section + 1;
   }
   const QStringList headers = {"Task name", "Status", "Created", "Last modified", "Completed at", "Organization", "Context", "Priority"};
   return section >= 0 && section < headers.size() ? QVariant(headers.at(section)) : QVariant();
}

Qt::ItemFlags PersistentTaskModel::flags(const QModelIndex &index) const {
   auto result = QAbstractTableModel::flags(index);
   if (index.isValid() && index.column() == STATUS_COLUMN && writable) {
      result |= Qt::ItemIsUserCheckable;
   }
   return result;
}

bool PersistentTaskModel::setData(const QModelIndex &index, const QVariant &value, int role) {
   if (!writable || !index.isValid() || index.row() < 0 || index.row() >= rowCount()
       || index.column() != STATUS_COLUMN || role != Qt::CheckStateRole
       || (value.toInt() != Qt::Checked && value.toInt() != Qt::Unchecked)) {
      return false;
   }
   const auto &task = tasks.at(index.row());
   const bool completed = value.toInt() == Qt::Checked;
   if (completed == (task.status == taskcompass::TaskStatus::COMPLETED)) {
      return false;
   }
   emit completionRequested(QString::fromStdString(task.id), completed);
   return true; // No optimistic mutation; refresh from committed result.
}

void PersistentTaskModel::replaceTasks(const std::vector<taskcompass::Task> &replacement) {
   replaceWorkspace({replacement, {}});
}
void PersistentTaskModel::replaceWorkspace(const taskcompass::WorkspaceSnapshot &replacement) {
   std::unique_ptr<taskcompass::OrganizationRules> replacementGraph;
   if (!replacement.organizations.empty()) {
      replacementGraph = std::make_unique<taskcompass::OrganizationRules>(taskcompass::organizationFields(replacement.organizations.front()).datasetId, replacement.organizations);
   }
   beginResetModel();
   graph = std::move(replacementGraph);
   contextSnapshot.contexts = replacement.contexts;
   contextSnapshot.taskContexts = replacement.taskContexts;
   contextSnapshot.schemaVersion = replacement.schemaVersion;
   contextIndexes.clear();
   std::unordered_map<std::string, std::size_t> contexts;
   for (std::size_t index = 0; index < contextSnapshot.contexts.size(); ++index) {
      const auto &context = contextSnapshot.contexts[index];
      if (!context.deletedAt) { contexts.emplace(context.id, index); }
   }
   for (const auto &link : contextSnapshot.taskContexts) {
      const auto context = contexts.find(link.contextId);
      if (!link.deletedAt && context != contexts.end()) { contextIndexes.emplace(link.taskId, context->second); }
   }
   tasks = replacement.tasks;
   std::erase_if(tasks, [](const auto &task) { return task.deletedAt.has_value(); });
   endResetModel();
}
bool PersistentTaskModel::effectivelyArchived(int row) const { return graph && graph->taskArchived(tasks.at(row)); }
bool PersistentTaskModel::matchesScope(int row, const std::optional<taskcompass::OrganizationKey> &scope, bool independentOnly) const {
   const auto &task = tasks.at(row);
   if (independentOnly) { return task.owner.kind == taskcompass::TaskOwnerKind::INDEPENDENT; }
   if (!scope) { return true; }
   if (!graph || task.owner.kind == taskcompass::TaskOwnerKind::INDEPENDENT) { return false; }
   const taskcompass::OrganizationKey owner{task.owner.kind == taskcompass::TaskOwnerKind::OBJECTIVE ? taskcompass::OrganizationKind::OBJECTIVE : taskcompass::OrganizationKind::PROJECT, task.owner.id};
   const auto ancestors = graph->path(owner);
   return std::find(ancestors.begin(), ancestors.end(), *scope) != ancestors.end();
}
const taskcompass::Task &PersistentTaskModel::taskAt(int row) const { return tasks.at(row); }
const taskcompass::Context *PersistentTaskModel::contextAt(int row, bool includeArchived) const {
   const auto found = contextIndexes.find(taskAt(row).id);
   if (found == contextIndexes.end()) { return nullptr; }
   const auto &context = contextSnapshot.contexts[found->second];
   return includeArchived || !context.archivedAt ? &context : nullptr;
}
bool PersistentTaskModel::hasContexts() const { return contextSnapshot.schemaVersion == 3; }
void PersistentTaskModel::setArchivedContextsVisible(bool enabled) {
   if (archivedContextsVisible == enabled) { return; }
   archivedContextsVisible = enabled;
   if (rowCount()) { emit dataChanged(index(0, CONTEXT_COLUMN), index(rowCount() - 1, CONTEXT_COLUMN)); }
}
void PersistentTaskModel::setWritable(bool enabled) { writable = enabled; }
