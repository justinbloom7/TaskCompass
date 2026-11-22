#include "OrganizationEditDialog.h"
#include "OrganizationMoveDialog.h"
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QStringList>

using namespace taskcompass;
OrganizationEditDialog::OrganizationEditDialog(const WorkspaceSnapshot &snapshot, OrganizationKind kind,
                                               const std::optional<OrganizationEntity> &original,
                                               std::optional<OrganizationKey> parentChoice, QWidget *parent)
   : QDialog(parent), title(new QLineEdit(this)), description(new QTextEdit(this)) {
   setWindowTitle(original ? "Edit organization" : "New organization");
   resize(520, 360);
   title->setObjectName("organizationTitle"); description->setObjectName("organizationDescription"); description->setAcceptRichText(false);
   request.kind = kind; request.parent = parentChoice; request.expectedWorkspace = workspaceRevisions(snapshot);
   if (original) {
      request.operation = OrganizationOperation::EDIT; request.id = organizationFields(*original).id;
      request.expectedRevision = organizationFields(*original).localRevision;
      title->setText(QString::fromStdString(organizationTitle(*original)));
      originalDescription = organizationFields(*original).description;
      description->setPlainText(QString::fromStdString(organizationFields(*original).description.value_or("")));
   }
   auto *layout = new QVBoxLayout(this);
   auto *form = new QFormLayout;
   form->addRow("&Name:", title); form->addRow("&Description:", description); layout->addLayout(form);
   auto *choose = new QPushButton("Choose Parent...", this);
   auto *parentText = new QLineEdit(this); parentText->setReadOnly(true); parentText->setObjectName("organizationParent");
   form->addRow("Parent:", parentText);
   choose->setEnabled(!original); // Parent changes use the explicit Move workflow.
   layout->addWidget(choose);
   auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
   auto update = [this, buttons, parentText, snapshot]() {
      if (!request.parent) { parentText->setText("Root pillar"); }
      else if (!snapshot.organizations.empty()) {
         taskcompass::OrganizationRules graph(taskcompass::organizationFields(snapshot.organizations.front()).datasetId, snapshot.organizations);
         QStringList path;
         for (const auto &key : graph.path(*request.parent)) { path.append(QString::fromStdString(taskcompass::organizationTitle(graph.find(key)))); }
         parentText->setText(path.join(" / "));
      }
      buttons->button(QDialogButtonBox::Ok)->setEnabled(!title->text().trimmed().isEmpty()
         && (request.operation == OrganizationOperation::EDIT || request.kind == OrganizationKind::PILLAR || request.parent.has_value()));
   };
   connect(choose, &QPushButton::clicked, this, [this, snapshot, update]() {
      OrganizationMoveDialog picker(snapshot, request.kind, request.parent, {}, false, this);
      if (picker.exec() == QDialog::Accepted) { request.parent = picker.parentChoice(); }
      update();
   });
   connect(title, &QLineEdit::textChanged, this, [update]() { update(); });
   connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
   connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
   layout->addWidget(buttons); update();
}
OrganizationCommand OrganizationEditDialog::command() const {
   auto result = request;
   result.title = title->text().toStdString();
   const auto text = description->toPlainText().toStdString();
   result.description = text.empty() ? std::nullopt : std::optional(text);
   if (text == originalDescription.value_or("")) { result.description = originalDescription; }
   return result;
}
