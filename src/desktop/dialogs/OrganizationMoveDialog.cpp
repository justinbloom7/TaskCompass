#include "OrganizationMoveDialog.h"
#include "desktop/models/OrganizationTreeModel.h"
#include <QTreeView>
#include <QSortFilterProxyModel>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <algorithm>

using namespace taskcompass;
namespace {
class ParentFilter : public QSortFilterProxyModel {
public:
   std::optional<OrganizationKind> kind;
   std::optional<OrganizationKey> moving;
   std::unique_ptr<OrganizationRules> graph;
   bool allowArchived = false;
   Qt::ItemFlags flags(const QModelIndex &index) const override {
      auto result = QSortFilterProxyModel::flags(index);
      const auto source = mapToSource(index);
      const auto *model = static_cast<OrganizationTreeModel *>(sourceModel());
      const auto key = model->keyAt(source);
      const bool rootAllowed = !kind || *kind == OrganizationKind::PILLAR;
      const bool validKind = key ? (!kind ? key->kind != OrganizationKind::PILLAR : key->kind != OrganizationKind::OBJECTIVE)
                                 : rootAllowed;
      if (!validKind || (!allowArchived && source.data(OrganizationTreeModel::EFFECTIVE_ARCHIVE_ROLE).toBool())) {
         result &= ~Qt::ItemIsSelectable;
      }
      return result;
   }
   QVariant data(const QModelIndex &index, int role) const override {
      if (role == Qt::DisplayRole && !index.data(OrganizationTreeModel::ID_ROLE).toString().size()) {
         return kind ? "No parent (root pillar)" : "Independent task (no owner)";
      }
      return QSortFilterProxyModel::data(index, role);
   }
protected:
   bool filterAcceptsRow(int row, const QModelIndex &parent) const override {
      const auto index = sourceModel()->index(row, 0, parent);
      if (index.data(OrganizationTreeModel::SCOPE_ROLE).toInt() == -1) { return false; }
      if (index.data(OrganizationTreeModel::SCOPE_ROLE).toInt() == 1) { return false; }
      const auto key = static_cast<OrganizationTreeModel *>(sourceModel())->keyAt(index);
      if (!key) { return !kind || *kind == OrganizationKind::PILLAR; }
      if (kind && (key->kind == OrganizationKind::OBJECTIVE || (*kind == OrganizationKind::PILLAR && key->kind != OrganizationKind::PILLAR))) { return false; }
      if (moving && graph) {
         const auto chain = graph->path(*key);
         if (std::find(chain.begin(), chain.end(), *moving) != chain.end()) { return false; }
      }
      return true;
   }
};
}
OrganizationMoveDialog::OrganizationMoveDialog(const WorkspaceSnapshot &snapshot, std::optional<OrganizationKind> kind,
                                               std::optional<OrganizationKey> currentParent, std::optional<OrganizationKey> moving,
                                               bool allowArchived, QWidget *parent)
   : QDialog(parent), tree(new QTreeView(this)), source(new OrganizationTreeModel(this)) {
   setWindowTitle(kind ? "Choose organizational parent" : "Choose task owner");
   resize(600, 440);
   tree->setObjectName("parentTree");
   source->replace(snapshot, true, false);
   auto *filter = new ParentFilter;
   filter->setParent(this); filter->kind = kind; filter->moving = moving; filter->allowArchived = allowArchived;
   if (!snapshot.organizations.empty()) { filter->graph = std::make_unique<OrganizationRules>(organizationFields(snapshot.organizations.front()).datasetId, snapshot.organizations); }
   filter->setSourceModel(source); proxy = filter;
   tree->setModel(proxy); tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
   auto *layout = new QVBoxLayout(this);
   layout->addWidget(new QLabel("Select a legal destination. Archived destinations are labeled; pillar rows are containers when choosing a task owner.", this));
   layout->addWidget(tree);
   auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
   auto update = [this, buttons]() {
      const auto index = tree->currentIndex();
      buttons->button(QDialogButtonBox::Ok)->setEnabled(index.isValid() && (proxy->flags(index) & Qt::ItemIsSelectable));
   };
   connect(tree->selectionModel(), &QItemSelectionModel::currentChanged, this, [update]() { update(); });
   connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
   connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
   layout->addWidget(buttons);
   QModelIndex initial = currentParent ? source->indexFor(*currentParent) : source->index(0, 0);
   auto mapped = proxy->mapFromSource(initial);
   for (auto ancestor = mapped.parent(); ancestor.isValid(); ancestor = ancestor.parent()) { tree->setExpanded(ancestor, true); }
   tree->setCurrentIndex(mapped); update();
}
std::optional<OrganizationKey> OrganizationMoveDialog::parentChoice() const { return source->keyAt(proxy->mapToSource(tree->currentIndex())); }
bool OrganizationMoveDialog::archivedDestination() const { return tree->currentIndex().data(OrganizationTreeModel::EFFECTIVE_ARCHIVE_ROLE).toBool(); }
