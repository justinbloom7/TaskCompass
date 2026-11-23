#pragma once

#include <QSortFilterProxyModel>
#include "application/WorkspaceRepository.h"
#include <functional>

class PersistentTaskFilter : public QSortFilterProxyModel {
public:
   using MovePreview = std::function<std::optional<taskcompass::TaskMovePreview>(const taskcompass::TaskMoveTarget &)>;
   using MoveCommit = std::function<void(const taskcompass::TaskMovePreview &)>;
   void setTaskMoveCallbacks(MovePreview preview, MoveCommit commit);
   void setTaskOrderingEnabled(bool enabled);
   Qt::ItemFlags flags(const QModelIndex &index) const override;
   QStringList mimeTypes() const override;
   QMimeData *mimeData(const QModelIndexList &indexes) const override;
   Qt::DropActions supportedDragActions() const override;
   Qt::DropActions supportedDropActions() const override;
   bool canDropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column,
                        const QModelIndex &parent) const override;
   bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column,
                     const QModelIndex &parent) override;
   std::optional<taskcompass::TaskMovePreview> previewTableDrop(const QMimeData *data, const QModelIndex &hit,
                                                              bool after) const;
   std::optional<taskcompass::TaskMovePreview> previewGroupDrop(const QMimeData *data, const std::string &contextId) const;
   void commitTaskMove(const taskcompass::TaskMovePreview &preview);
   void setIncludeCompleted(bool enabled);
   void setOrganizationScope(const std::optional<taskcompass::OrganizationKey> &scope, bool independentOnly);
   void setIncludeArchived(bool enabled);
protected:
   bool filterAcceptsRow(int row, const QModelIndex &parent) const override;
   bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
private:
   const taskcompass::Task &visibleTask(int row) const;
   std::string visibleContext(int row) const;
   std::optional<taskcompass::TaskMovePreview> previewDrop(const QMimeData *data, taskcompass::TaskMoveTarget target) const;
   MovePreview movePreview;
   MoveCommit moveCommit;
   bool orderingEnabled = false;
   bool includeCompleted = false;
   bool includeArchived = false;
   bool independentOnly = false;
   std::optional<taskcompass::OrganizationKey> scope;
};
