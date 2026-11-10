#include "TaskTableModel.h"

TaskTableModel::TaskTableModel(QObject *parent) : QAbstractTableModel(parent) {
   const QDate today = QDate::currentDate();
   // Context and filter tags are hand-authored presentation fixtures, not a domain model.
   // Sprint tags reflect an objective selection, never a scheduled task date.
   tasks = {
      {"Review household checklist", "Direct task in the Household project, with no objective.",
       "Active", "High", today, "Family / Household",
       {"pillars", "pillar:family", "project:household"}},
      {"Prepare council notes", "Task in Service focus, an objective directly under Council.",
       "Active", "Normal", today.addDays(1), "Council / Service focus",
       {"pillars", "pillar:council", "objectives", "objective:service", "sprints", "sprint:november2026"}},
      {"Outline prototype review", "Task in one objective owned by TaskCompass; selected in two sample sprints.",
       "On Hold", "Low", {}, "Learning / Study / TaskCompass / Complete desktop prototype",
       {"pillars", "pillar:learning", "pillar:study", "project:taskcompass", "objectives", "objective:prototype",
        "sprints", "sprint:november2026", "sprint:december2026"}},
      {"Review desktop notes", "Direct task in a nested subproject; no objective.",
       "Completed", "Normal", today.addDays(-1), "Learning / Study / TaskCompass / Desktop notes",
       {"pillars", "pillar:learning", "pillar:study", "project:taskcompass", "project:notes"}},
      {"Arrange family outing", "Task in Family outing, an objective owned by Household.",
       "Active", "High", today.addDays(3), "Family / Household / Family outing",
       {"pillars", "pillar:family", "project:household", "objectives", "objective:familyOuting"}},
      {"Capture an idea", "Independent inbox task; no objective, project or pillar owner.",
       "Active", "Normal", {}, "Inbox (independent)", {"inbox"}}
   };
}

int TaskTableModel::rowCount(const QModelIndex &parent) const {
   return parent.isValid() ? 0 : static_cast<int>(tasks.size());
}

int TaskTableModel::columnCount(const QModelIndex &parent) const {
   return parent.isValid() ? 0 : COLUMN_COUNT;
}

QVariant TaskTableModel::data(const QModelIndex &index, int role) const {
   if (!index.isValid() || index.row() < 0 || index.row() >= tasks.size()
       || index.column() < 0 || index.column() >= COLUMN_COUNT) {
      return {};
   }
   const SampleTask &task = tasks.at(index.row());
   if (role == NAVIGATION_TAGS_ROLE) {
      return task.navigationTags;
   }
   if (role == Qt::ToolTipRole) {
      return task.description;
   }
   if (role != Qt::DisplayRole && role != SORT_ROLE) {
      return {};
   }
   switch (index.column()) {
      case TITLE_COLUMN: return task.title;
      case STATUS_COLUMN: return task.status;
      case PRIORITY_COLUMN:
         if (role == SORT_ROLE) {
            return task.priority == "High" ? 0 : task.priority == "Normal" ? 1 : 2;
         }
         return task.priority;
      case DUE_DATE_COLUMN:
         if (role == SORT_ROLE) {
            return task.dueDate;
         }
         return task.dueDate.isValid() ? task.dueDate.toString(Qt::ISODate) : QString("None");
      case CONTEXT_COLUMN: return task.organizationContext;
      default: return {};
   }
}

QVariant TaskTableModel::headerData(int section, Qt::Orientation orientation,
                                    int role) const {
   if (role != Qt::DisplayRole) {
      return {};
   }
   if (orientation == Qt::Vertical) {
      return section + 1;
   }
   const QStringList headers = {"Task name", "Status", "Priority", "Due date", "Organization context"};
   return section >= 0 && section < headers.size() ? QVariant(headers.at(section)) : QVariant();
}

SampleTask TaskTableModel::taskAt(int row) const {
   return row >= 0 && row < tasks.size() ? tasks.at(row) : SampleTask();
}

bool TaskTableModel::updateTask(int row, const SampleTask &task) {
   if (row < 0 || row >= tasks.size() || task.title.trimmed().isEmpty()) {
      return false;
   }
   tasks[row] = task;
   tasks[row].title = task.title.trimmed();
   emit dataChanged(index(row, 0), index(row, COLUMN_COUNT - 1));
   return true;
}

int TaskTableModel::appendTask(const SampleTask &task) {
   if (task.title.trimmed().isEmpty()) {
      return -1;
   }
   const int row = rowCount();
   beginInsertRows(QModelIndex(), row, row);
   tasks.append(task);
   tasks.last().title = task.title.trimmed();
   endInsertRows();
   return row;
}
