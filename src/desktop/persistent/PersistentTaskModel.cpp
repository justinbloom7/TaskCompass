#include "PersistentTaskModel.h"

#include <QDateTime>
#include <QTimeZone>
#include <QStringList>

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
   const QStringList headers = {"Task name", "Status", "Created", "Last modified", "Completed at"};
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
   beginResetModel();
   tasks = replacement;
   endResetModel();
}
const taskcompass::Task &PersistentTaskModel::taskAt(int row) const { return tasks.at(row); }
void PersistentTaskModel::setWritable(bool enabled) { writable = enabled; }
