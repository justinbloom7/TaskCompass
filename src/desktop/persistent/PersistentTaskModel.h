#pragma once

#include "domain/Task.h"
#include <QAbstractTableModel>

class PersistentTaskModel : public QAbstractTableModel {
   Q_OBJECT
public:
   enum Column { TITLE_COLUMN, STATUS_COLUMN, CREATED_COLUMN, UPDATED_COLUMN, COMPLETED_COLUMN, COLUMN_COUNT };
   explicit PersistentTaskModel(QObject *parent = nullptr);
   int rowCount(const QModelIndex &parent = {}) const override;
   int columnCount(const QModelIndex &parent = {}) const override;
   QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
   QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
   Qt::ItemFlags flags(const QModelIndex &index) const override;
   bool setData(const QModelIndex &index, const QVariant &value, int role) override;
   void replaceTasks(const std::vector<taskcompass::Task> &tasks);
   const taskcompass::Task &taskAt(int row) const;
   void setWritable(bool enabled);

signals:
   void completionRequested(const QString &id, bool completed);

private:
   std::vector<taskcompass::Task> tasks;
   bool writable = false;
};
