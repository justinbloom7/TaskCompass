#include "TaskEditDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

TaskEditDialog::TaskEditDialog(const SampleTask &task, QWidget *parent)
   : QDialog(parent), originalTask(task) {
   setWindowTitle("Edit sample task — memory only");
   resize(480, 360);
   auto *layout = new QVBoxLayout(this);
   auto *form = new QFormLayout;
   titleEdit = new QLineEdit(task.title, this);
   titleEdit->setObjectName("titleEdit");
   descriptionEdit = new QTextEdit(this);
   descriptionEdit->setObjectName("descriptionEdit");
   descriptionEdit->setAcceptRichText(false);
   descriptionEdit->setPlainText(task.description);
   statusEdit = new QComboBox(this);
   statusEdit->setObjectName("statusEdit");
   statusEdit->addItems({"Active", "On Hold", "Completed"});
   statusEdit->setCurrentText(task.status);
   hasDueDate = new QCheckBox("Has a due date", this);
   hasDueDate->setObjectName("hasDueDate");
   hasDueDate->setChecked(task.dueDate.isValid());
   dueDateEdit = new QDateEdit(task.dueDate.isValid() ? task.dueDate : QDate::currentDate(), this);
   dueDateEdit->setObjectName("dueDateEdit");
   dueDateEdit->setCalendarPopup(true);
   dueDateEdit->setDisplayFormat("yyyy-MM-dd");
   dueDateEdit->setEnabled(hasDueDate->isChecked());
   form->addRow("&Title:", titleEdit);
   form->addRow("&Description:", descriptionEdit);
   form->addRow("&Status (sample labels):", statusEdit);
   form->addRow(hasDueDate);
   form->addRow("D&ue date:", dueDateEdit);
   layout->addLayout(form);
   auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
   layout->addWidget(buttons);
   buttons->button(QDialogButtonBox::Ok)->setEnabled(!task.title.trimmed().isEmpty());
   connect(titleEdit, &QLineEdit::textChanged, this, [buttons](const QString &title) {
      buttons->button(QDialogButtonBox::Ok)->setEnabled(!title.trimmed().isEmpty());
   });
   connect(hasDueDate, &QCheckBox::toggled, dueDateEdit, &QWidget::setEnabled);
   connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
   connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
   titleEdit->setFocus();
}

SampleTask TaskEditDialog::editedTask() const {
   SampleTask task = originalTask;
   task.title = titleEdit->text().trimmed();
   task.description = descriptionEdit->toPlainText();
   task.status = statusEdit->currentText();
   task.dueDate = hasDueDate->isChecked() ? dueDateEdit->date() : QDate();
   return task;
}
