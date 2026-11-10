#pragma once

#include "desktop/models/SampleTask.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDateEdit;
class QLineEdit;
class QTextEdit;

class TaskEditDialog : public QDialog {
public:
   explicit TaskEditDialog(const SampleTask &task, QWidget *parent = nullptr);
   SampleTask editedTask() const;

private:
   SampleTask originalTask;
   QLineEdit *titleEdit;
   QTextEdit *descriptionEdit;
   QComboBox *statusEdit;
   QCheckBox *hasDueDate;
   QDateEdit *dueDateEdit;
};
