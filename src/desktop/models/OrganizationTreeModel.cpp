#include "OrganizationTreeModel.h"
#include <QStringList>
#include <algorithm>

using namespace taskcompass;

OrganizationTreeModel::OrganizationTreeModel(QObject *parent) : QAbstractItemModel(parent) {}
OrganizationTreeModel::Node *OrganizationTreeModel::node(const QModelIndex &index) const {
   return index.isValid() ? static_cast<Node *>(index.internalPointer()) : const_cast<Node *>(&root);
}
QModelIndex OrganizationTreeModel::index(int row, int column, const QModelIndex &parent) const {
   if (column != 0 || row < 0 || (parent.isValid() && parent.column() != 0)) { return {}; }
   const auto *item = node(parent);
   return row < static_cast<int>(item->children.size()) ? createIndex(row, column, item->children[row]) : QModelIndex();
}
QModelIndex OrganizationTreeModel::parent(const QModelIndex &index) const {
   if (!index.isValid()) { return {}; }
   const auto *item = node(index)->parent;
   return item && item != &root ? createIndex(item->row, 0, const_cast<Node *>(item)) : QModelIndex();
}
int OrganizationTreeModel::rowCount(const QModelIndex &parent) const {
   return parent.isValid() && parent.column() != 0 ? 0 : static_cast<int>(node(parent)->children.size());
}
int OrganizationTreeModel::columnCount(const QModelIndex &) const { return 1; }
QVariant OrganizationTreeModel::headerData(int, Qt::Orientation orientation, int role) const {
   return orientation == Qt::Horizontal && role == Qt::DisplayRole ? QVariant("Organization") : QVariant();
}
QVariant OrganizationTreeModel::data(const QModelIndex &index, int role) const {
   if (!index.isValid()) { return {}; }
   const auto *item = node(index);
   if (role == ID_ROLE) { return item->key ? QString::fromStdString(item->key->id) : QString(); }
   if (role == KIND_ROLE) { return item->key ? static_cast<int>(item->key->kind) : -1; }
   if (role == SCOPE_ROLE) { return item->scope; }
   if (role == EXPLICIT_ARCHIVE_ROLE) { return item->explicitArchive; }
   if (role == EFFECTIVE_ARCHIVE_ROLE) { return item->effectiveArchive; }
   if (role == PATH_ROLE) { return item->key ? pathFor(*item->key) : item->title; }
   if (role == Qt::DisplayRole) { return item->title + (item->explicitArchive ? " [archived]" : item->effectiveArchive ? " [inherited archive]" : ""); }
   if (role == Qt::ToolTipRole && item->key) { return pathFor(*item->key) + "\n" + QString::fromStdString(item->key->id); }
   return {};
}
QString OrganizationTreeModel::pathFor(const OrganizationKey &key) const {
   if (!graph) { return {}; }
   QStringList parts;
   for (const auto &ancestor : graph->path(key)) { parts.append(displayTitles.at(ancestor)); }
   return parts.join(" / ");
}
void OrganizationTreeModel::replace(const WorkspaceSnapshot &snapshot, bool showArchived, bool objectiveView) {
   std::unique_ptr<OrganizationRules> replacement;
   if (!snapshot.organizations.empty()) {
      replacement = std::make_unique<OrganizationRules>(organizationFields(snapshot.organizations.front()).datasetId, snapshot.organizations);
   }
   beginResetModel();
   graph = std::move(replacement); root.children.clear(); byKey.clear(); nodes.clear(); displayTitles.clear();
   std::map<QString, std::size_t> titleCounts;
   for (const auto &entity : snapshot.organizations) {
      ++titleCounts[QString::fromStdString(organizationTitle(entity)).toCaseFolded()];
   }
   for (const auto &entity : snapshot.organizations) {
      const auto key = organizationKey(entity);
      auto title = QString::fromStdString(organizationTitle(entity));
      if (titleCounts.at(title.toCaseFolded()) > 1) {
         static constexpr const char *KINDS[] = {"pillar", "project", "objective"};
         title += QString(" [%1, %2]").arg(KINDS[static_cast<int>(key.kind)], QString::fromStdString(key.id));
      }
      displayTitles[key] = title;
   }
   auto add = [&](QString title, int scope) {
      auto item = std::make_unique<Node>(); item->title = std::move(title); item->scope = scope; item->parent = &root;
      root.children.push_back(item.get()); nodes.push_back(std::move(item));
   };
   add("All tasks", 0); add("Inbox — independent tasks", 1);
   for (const auto &entity : snapshot.organizations) {
      const auto &fields = organizationFields(entity);
      const auto key = organizationKey(entity);
      if (fields.deletedAt || (objectiveView && key.kind != OrganizationKind::OBJECTIVE)) { continue; }
      const bool archived = graph->effectivelyArchived(key);
      if (!showArchived && archived) { continue; }
      auto item = std::make_unique<Node>(); item->key = key; item->title = displayTitles.at(key);
      item->explicitArchive = fields.archivedAt.has_value(); item->effectiveArchive = archived;
      if (objectiveView) { item->title += " — " + pathFor(key); }
      byKey[key] = item.get(); nodes.push_back(std::move(item));
   }
   for (const auto &[key, item] : byKey) {
      const auto owner = organizationParent(graph->find(key));
      item->parent = !objectiveView && owner && byKey.contains(*owner) ? byKey.at(*owner) : &root;
      item->parent->children.push_back(item);
   }
   std::vector<Node *> parents{&root};
   for (const auto &item : nodes) { parents.push_back(item.get()); }
   for (auto *item : parents) {
      std::stable_sort(item->children.begin(), item->children.end(), [](const Node *first, const Node *second) {
         if (first->scope != second->scope) { return first->scope < second->scope; }
         const auto compare = QString::compare(first->title, second->title, Qt::CaseInsensitive);
         return compare == 0 && first->key && second->key ? *first->key < *second->key : compare < 0;
      });
      for (std::size_t index = 0; index < item->children.size(); ++index) { item->children[index]->row = static_cast<int>(index); }
   }
   endResetModel();
}
QModelIndex OrganizationTreeModel::indexFor(const OrganizationKey &key) const {
   const auto found = byKey.find(key);
   return found == byKey.end() ? QModelIndex() : createIndex(found->second->row, 0, found->second);
}
std::vector<QModelIndex> OrganizationTreeModel::organizationIndexes() const {
   std::vector<QModelIndex> result;
   for (const auto &[key, item] : byKey) { (void)key; result.push_back(createIndex(item->row, 0, item)); }
   return result;
}
std::optional<OrganizationKey> OrganizationTreeModel::keyAt(const QModelIndex &index) const {
   return index.isValid() ? node(index)->key : std::nullopt;
}
