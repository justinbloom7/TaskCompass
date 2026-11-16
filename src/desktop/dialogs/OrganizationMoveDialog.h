#pragma once

#include "application/WorkspaceRepository.h"
#include <QDialog>

class QTreeView;
class OrganizationTreeModel;
class QSortFilterProxyModel;

class OrganizationMoveDialog : public QDialog {
   Q_OBJECT
public:
   OrganizationMoveDialog(const taskcompass::WorkspaceSnapshot &snapshot,
                          std::optional<taskcompass::OrganizationKind> kind,
                          std::optional<taskcompass::OrganizationKey> currentParent,
                          std::optional<taskcompass::OrganizationKey> moving,
                          bool allowArchived, QWidget *parent = nullptr);
   std::optional<taskcompass::OrganizationKey> parentChoice() const;
   bool archivedDestination() const;
private:
   QTreeView *tree;
   OrganizationTreeModel *source;
   QSortFilterProxyModel *proxy;
};
