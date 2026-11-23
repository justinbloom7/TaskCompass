// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "application/WorkspaceRepository.h"
#include <QTableView>
#include <QPointer>
#include <QTimer>

class QListWidget;
class QFrame;
class QMimeData;
class PersistentTaskFilter;

class PersistentTaskOrderTable : public QTableView {
   Q_OBJECT
public:
   using QTableView::QTableView;
signals:
   void dragFinished();
protected:
   void startDrag(Qt::DropActions supported) override;
};

class PersistentTaskOrderView : public QObject {
   Q_OBJECT
public:
   using Describe = std::function<QString(const taskcompass::TaskMovePreview &)>;
   PersistentTaskOrderView(PersistentTaskOrderTable *table, QListWidget *groups,
                           PersistentTaskFilter *proxy, Describe describe);
   void setEnabled(bool enabled);
   void clearPreview();

signals:
   void previewChanged(const QString &text);

protected:
   bool eventFilter(QObject *watched, QEvent *event) override;

private:
   struct Offer {
      taskcompass::TaskMovePreview preview;
      QModelIndex hit;
      bool after;
      int y;
   };
   std::optional<Offer> offerAt(QAbstractItemView *surface, const QPoint &position, const QMimeData *data) const;
   void showOffer(QAbstractItemView *surface, const Offer &offer);
   void scroll();
   PersistentTaskOrderTable *table;
   QListWidget *groups;
   PersistentTaskFilter *proxy;
   Describe describe;
   QFrame *line;
   QTimer scrollTimer;
   QPointer<QAbstractItemView> surface;
   QPointer<const QMimeData> mime;
   QPoint position;
   std::optional<taskcompass::TaskMovePreview> displayed;
   bool enabled = false;
};
