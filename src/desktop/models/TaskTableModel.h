#pragma once

#include "SampleTask.h"

#include <QAbstractTableModel>
#include <QVector>

class TaskTableModel : public QAbstractTableModel {
   Q_OBJECT

public:
   enum Column {
      TITLE_COLUMN,
      STATUS_COLUMN,
      PRIORITY_COLUMN,
      DUE_DATE_COLUMN,
      CONTEXT_COLUMN,
      COLUMN_COUNT
   };
   static constexpr int SORT_ROLE = Qt::UserRole + 1;
   static constexpr int NAVIGATION_TAGS_ROLE = Qt::UserRole + 2;

   explicit TaskTableModel(QObject *parent = nullptr);
   int rowCount(const QModelIndex &parent = QModelIndex()) const override;
   int columnCount(const QModelIndex &parent = QModelIndex()) const override;
   QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
   QVariant headerData(int section, Qt::Orientation orientation,
                       int role = Qt::DisplayRole) const override;
   SampleTask taskAt(int row) const;
   bool updateTask(int row, const SampleTask &task);
   int appendTask(const SampleTask &task);

private:
   QVector<SampleTask> tasks;
};
