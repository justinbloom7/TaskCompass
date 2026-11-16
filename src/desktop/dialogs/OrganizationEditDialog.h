#pragma once

#include "application/WorkspaceRepository.h"
#include <QDialog>

class QLineEdit;
class QTextEdit;

class OrganizationEditDialog : public QDialog {
   Q_OBJECT
public:
   OrganizationEditDialog(const taskcompass::WorkspaceSnapshot &snapshot,
                          taskcompass::OrganizationKind kind,
                          const std::optional<taskcompass::OrganizationEntity> &original,
                          std::optional<taskcompass::OrganizationKey> parentChoice,
                          QWidget *parent = nullptr);
   taskcompass::OrganizationCommand command() const;
private:
   QLineEdit *title;
   QTextEdit *description;
   taskcompass::OrganizationCommand request;
   std::optional<std::string> originalDescription;
};
