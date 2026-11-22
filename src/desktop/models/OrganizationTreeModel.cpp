#include "OrganizationTreeModel.h"
#include <QStringList>
#include <QMimeData>
#include <QUuid>
#include <algorithm>
#include <QApplication>
#include <QIconEngine>
#include <QPainter>
#include <QPalette>
#include <QPainterPath>

using namespace taskcompass;

namespace {
// Application-owned vector geometry, rendered at the requested size and palette.
class EntityIconEngine : public QIconEngine {
public:
   EntityIconEngine(OrganizationKind kind, bool nested) : kind(kind), nested(nested) {}
   QIconEngine *clone() const override { return new EntityIconEngine(kind, nested); }
   void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State) override {
      painter->save();
      painter->setRenderHint(QPainter::Antialiasing);
      painter->translate(rect.topLeft());
      painter->scale(rect.width() / 24.0, rect.height() / 24.0);
      const auto palette = QApplication::palette();
      const auto group = mode == QIcon::Disabled ? QPalette::Disabled : QPalette::Active;
      painter->setPen(QPen(palette.color(group, mode == QIcon::Selected ? QPalette::HighlightedText : QPalette::Text), 1.8));
      painter->setBrush(Qt::NoBrush);
      if (kind == OrganizationKind::PILLAR) {
         painter->drawLine(4, 5, 20, 5); painter->drawLine(4, 19, 20, 19);
         painter->drawRect(QRectF(7, 7, 3, 10)); painter->drawRect(QRectF(14, 7, 3, 10));
      } else if (kind == OrganizationKind::PROJECT) {
         QPainterPath folder; folder.moveTo(3, 7); folder.lineTo(10, 7); folder.lineTo(12, 10);
         folder.lineTo(21, 10); folder.lineTo(21, 19); folder.lineTo(3, 19); folder.closeSubpath();
         painter->drawPath(folder);
      } else {
         painter->drawEllipse(QRectF(4, 4, 16, 16));
         painter->drawEllipse(QRectF(9, 9, 6, 6));
         painter->drawLine(12, 12, 20, 4);
      }
      if (nested) { painter->drawLine(2, 2, 2, 5); painter->drawLine(2, 2, 5, 2); }
      painter->restore();
   }
   QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override {
      QPixmap result(size); result.fill(Qt::transparent);
      QPainter painter(&result); paint(&painter, QRect(QPoint(), size), mode, state); return result;
   }
private:
   OrganizationKind kind;
   bool nested;
};
}


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
   if (role == OPEN_COUNT_ROLE) { return QVariant::fromValue(item->openCount); }
   if (role == Qt::DecorationRole && item->key) {
      return QIcon(new EntityIconEngine(item->key->kind, item->type == "Subpillar" || item->type == "Subproject"));
   }
   if (role == TYPE_ROLE) { return item->type; }
   if (role == ID_ROLE) { return item->key ? QString::fromStdString(item->key->id) : QString(); }
   if (role == KIND_ROLE) { return item->key ? static_cast<int>(item->key->kind) : -1; }
   if (role == SCOPE_ROLE) { return item->scope; }
   if (role == EXPLICIT_ARCHIVE_ROLE) { return item->explicitArchive; }
   if (role == EFFECTIVE_ARCHIVE_ROLE) { return item->effectiveArchive; }
   if (role == PATH_ROLE) { return item->key ? pathFor(*item->key) : item->title; }
   if (role == Qt::DisplayRole) { return item->title + (item->key ? QString(" (%1)").arg(item->openCount) : QString()) + (item->explicitArchive ? " [archived]" : item->effectiveArchive ? " [inherited archive]" : ""); }
   if ((role == Qt::ToolTipRole || role == Qt::AccessibleTextRole || role == Qt::AccessibleDescriptionRole) && item->key) { return item->type + "\n" + pathFor(*item->key) + "\n" + QString::fromStdString(item->key->id)
      + QString("\nOpen tasks: %1").arg(item->openCount) + (item->explicitArchive ? "\nArchived" : item->effectiveArchive ? "\nInherited archive" : ""); }
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
   struct Total {
      std::optional<OrganizationKey> parent;
      std::vector<OrganizationKey> children;
      qulonglong count = 0;
      bool archived = false;
      bool removed = false;
   };
   std::map<OrganizationKey, Total> totals;
   for (const auto &entity : snapshot.organizations) {
      const auto &fields = organizationFields(entity);
      totals[organizationKey(entity)] = {organizationParent(entity), {}, 0, fields.archivedAt.has_value(), fields.deletedAt.has_value()};
   }
   std::vector<OrganizationKey> order;
   for (const auto &[key, total] : totals) {
      if (total.parent) { totals.at(*total.parent).children.push_back(key); }
      else { order.push_back(key); }
   }
   // Iterative top-down inheritance, then one bottom-up subtree aggregation.
   for (std::size_t i = 0; i < order.size(); ++i) {
      const auto &parent = totals.at(order[i]);
      for (const auto &key : parent.children) {
         auto &child = totals.at(key);
         child.archived = child.archived || parent.archived;
         child.removed = child.removed || parent.removed;
         order.push_back(key);
      }
   }
   for (const auto &task : snapshot.tasks) {
      if (task.status != TaskStatus::ACTIVE || task.deletedAt || task.owner.kind == TaskOwnerKind::INDEPENDENT) { continue; }
      const OrganizationKey owner{task.owner.kind == TaskOwnerKind::OBJECTIVE ? OrganizationKind::OBJECTIVE : OrganizationKind::PROJECT, task.owner.id};
      auto &total = totals.at(owner);
      if (!total.archived && !total.removed) { ++total.count; }
   }
   for (auto key = order.rbegin(); key != order.rend(); ++key) {
      const auto &total = totals.at(*key);
      if (total.parent) { totals.at(*total.parent).count += total.count; }
   }
   beginResetModel();
   dragToken = QUuid::createUuid().toString();
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
   add("──────── Pillars ────────", -1);
   for (const auto &entity : snapshot.organizations) {
      const auto &fields = organizationFields(entity);
      const auto key = organizationKey(entity);
      if (fields.deletedAt || (objectiveView && key.kind != OrganizationKind::OBJECTIVE)) { continue; }
      const bool archived = totals.at(key).archived;
      if (!showArchived && archived) { continue; }
      auto item = std::make_unique<Node>(); item->key = key; item->title = displayTitles.at(key);
      item->sortTitle = QString::fromStdString(organizationTitle(entity));
      const auto parent = organizationParent(entity);
      item->type = key.kind == OrganizationKind::PILLAR ? (parent ? "Subpillar" : "Pillar")
         : key.kind == OrganizationKind::PROJECT ? (parent && parent->kind == OrganizationKind::PROJECT ? "Subproject" : "Project") : "Objective";
      item->explicitArchive = fields.archivedAt.has_value(); item->effectiveArchive = archived;
      item->openCount = totals.at(key).count;
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
         auto group = [](const Node *entry) { return entry->key ? 3 + static_cast<int>(entry->key->kind) : entry->scope == -1 ? 2 : entry->scope; };
         if (group(first) != group(second)) { return group(first) < group(second); }
         const auto compare = QString::compare(first->sortTitle, second->sortTitle, Qt::CaseInsensitive);
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
   return index.isValid() && index.model() == this ? node(index)->key : std::nullopt;
}

Qt::ItemFlags OrganizationTreeModel::flags(const QModelIndex &index) const {
   if (!index.isValid() || index.data(SCOPE_ROLE).toInt() == -1) { return Qt::NoItemFlags; }
   auto result = QAbstractItemModel::flags(index);
   if (reparentingEnabled && keyAt(index)) {
      result |= Qt::ItemIsDragEnabled;
      if (keyAt(index)->kind != OrganizationKind::OBJECTIVE) { result |= Qt::ItemIsDropEnabled; }
   }
   return result;
}
void OrganizationTreeModel::setReparentingEnabled(bool enabled) { reparentingEnabled = enabled; }
QStringList OrganizationTreeModel::mimeTypes() const { return {"application/x-taskcompass-organization"}; }
Qt::DropActions OrganizationTreeModel::supportedDropActions() const { return Qt::MoveAction; }
QMimeData *OrganizationTreeModel::mimeData(const QModelIndexList &indexes) const {
   auto *result = new QMimeData;
   if (reparentingEnabled && indexes.size() == 1) {
      if (const auto key = keyAt(indexes.front())) {
         result->setData(mimeTypes().front(), (dragToken + ":" + QString::number(static_cast<int>(key->kind)) + ":" + QString::fromStdString(key->id)).toUtf8());
      }
   }
   return result;
}
bool OrganizationTreeModel::canDropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) const {
   if (!reparentingEnabled || !graph || action != Qt::MoveAction || row != -1 || column > 0 || !keyAt(parent)) { return false; }
   const auto payload = QString::fromUtf8(data->data(mimeTypes().front()));
   for (const auto &[key, item] : byKey) {
      (void)item;
      if (payload != dragToken + ":" + QString::number(static_cast<int>(key.kind)) + ":" + QString::fromStdString(key.id)) { continue; }
      try {
         auto entity = graph->find(key);
         if (organizationParent(entity) == keyAt(parent)) { return false; }
         setOrganizationParent(entity, keyAt(parent));
         graph->requireAvailableParent(entity, true);
         return true;
      } catch (const std::exception &) { return false; }
   }
   return false;
}
bool OrganizationTreeModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) {
   if (!canDropMimeData(data, action, row, column, parent)) { return false; }
   const auto payload = QString::fromUtf8(data->data(mimeTypes().front()));
   for (const auto &[key, item] : byKey) {
      if (payload == dragToken + ":" + QString::number(static_cast<int>(key.kind)) + ":" + QString::fromStdString(key.id)) {
         emit reparentRequested(createIndex(item->row, 0, item), parent);
         return true; // Intent only: existing Move preview and command own persistence.
      }
   }
   return false;
}
