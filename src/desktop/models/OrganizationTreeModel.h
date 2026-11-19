#pragma once

#include "application/WorkspaceRepository.h"
#include <QAbstractItemModel>
#include <memory>

class OrganizationTreeModel : public QAbstractItemModel {
   Q_OBJECT
public:
   enum Role { ID_ROLE = Qt::UserRole, KIND_ROLE, SCOPE_ROLE, EXPLICIT_ARCHIVE_ROLE, EFFECTIVE_ARCHIVE_ROLE, PATH_ROLE, TYPE_ROLE, OPEN_COUNT_ROLE };
   explicit OrganizationTreeModel(QObject *parent = nullptr);
   QModelIndex index(int row, int column, const QModelIndex &parent = {}) const override;
   QModelIndex parent(const QModelIndex &index) const override;
   int rowCount(const QModelIndex &parent = {}) const override;
   int columnCount(const QModelIndex &parent = {}) const override;
   QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
   QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
   Qt::ItemFlags flags(const QModelIndex &index) const override;
   QStringList mimeTypes() const override;
   QMimeData *mimeData(const QModelIndexList &indexes) const override;
   bool canDropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) const override;
   bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) override;
   Qt::DropActions supportedDropActions() const override;
   void setReparentingEnabled(bool enabled);
   void replace(const taskcompass::WorkspaceSnapshot &snapshot, bool showArchived, bool objectiveView);
   QModelIndex indexFor(const taskcompass::OrganizationKey &key) const;
   std::vector<QModelIndex> organizationIndexes() const;
   std::optional<taskcompass::OrganizationKey> keyAt(const QModelIndex &index) const;
   QString pathFor(const taskcompass::OrganizationKey &key) const;
signals:
   void reparentRequested(const QModelIndex &source, const QModelIndex &destination);
private:
   bool reparentingEnabled = false;
   QString dragToken;
   struct Node {
      std::optional<taskcompass::OrganizationKey> key;
      QString title;
      QString type;
      QString sortTitle;
      qulonglong openCount = 0;
      Node *parent = nullptr;
      std::vector<Node *> children;
      int row = 0;
      int scope = 2;
      bool explicitArchive = false;
      bool effectiveArchive = false;
   };
   Node *node(const QModelIndex &index) const;
   Node root;
   // Flat ownership: destruction never recursively follows a deep hierarchy.
   std::vector<std::unique_ptr<Node>> nodes;
   std::map<taskcompass::OrganizationKey, Node *> byKey;
   std::map<taskcompass::OrganizationKey, QString> displayTitles;
   std::unique_ptr<taskcompass::OrganizationRules> graph;
};
