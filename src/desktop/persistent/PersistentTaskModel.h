#pragma once

#include "domain/Task.h"
#include "application/WorkspaceRepository.h"
#include <QAbstractTableModel>
#include <memory>
#include <unordered_map>

class PersistentTaskModel : public QAbstractTableModel {
   Q_OBJECT
public:
   enum Column { TITLE_COLUMN, STATUS_COLUMN, CREATED_COLUMN, UPDATED_COLUMN, COMPLETED_COLUMN, ORGANIZATION_COLUMN, CONTEXT_COLUMN, PRIORITY_COLUMN, COLUMN_COUNT };
   explicit PersistentTaskModel(QObject *parent = nullptr);
   int rowCount(const QModelIndex &parent = {}) const override;
   int columnCount(const QModelIndex &parent = {}) const override;
   QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
   QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
   Qt::ItemFlags flags(const QModelIndex &index) const override;
   bool setData(const QModelIndex &index, const QVariant &value, int role) override;
   void replaceTasks(const std::vector<taskcompass::Task> &tasks);
   void replaceWorkspace(const taskcompass::WorkspaceSnapshot &replacement);
   bool effectivelyArchived(int row) const;
   bool matchesScope(int row, const std::optional<taskcompass::OrganizationKey> &scope, bool independentOnly) const;
   const taskcompass::Task &taskAt(int row) const;
   const taskcompass::Context *contextAt(int row, bool includeArchived) const;
   void setArchivedContextsVisible(bool enabled);
   bool hasContexts() const;
   void setWritable(bool enabled);

signals:
   void completionRequested(const QString &id, bool completed);

private:
   std::unique_ptr<taskcompass::OrganizationRules> graph;
   std::vector<taskcompass::Task> tasks;
   taskcompass::WorkspaceSnapshot contextSnapshot;
   std::unordered_map<std::string, std::size_t> contextIndexes;
   bool archivedContextsVisible = false;
   bool writable = false;
};
