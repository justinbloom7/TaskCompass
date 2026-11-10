#pragma once

#include <QDate>
#include <QString>
#include <QStringList>

// Disposable presentation fixture, not the TaskCompass domain model.
struct SampleTask {
   QString title;
   QString description;
   QString status = "Active";
   QString priority = "Normal";
   QDate dueDate;
   QString organizationContext; // Display path only, not stored domain ownership.
   QStringList navigationTags; // Static fixture filters, including ancestor/view tags.
};
