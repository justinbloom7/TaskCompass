// SPDX-License-Identifier: GPL-3.0-or-later
#include "PersistentTaskOrderView.h"
#include "PersistentTaskFilter.h"
#include <QDrag>
#include <QDragEnterEvent>
#include <QFrame>
#include <QListWidget>
#include <QMimeData>
#include <QScrollBar>
#include <algorithm>

void PersistentTaskOrderTable::startDrag(Qt::DropActions supported) {
   if (!(supported & Qt::MoveAction)) { return; }
   auto *data = model()->mimeData(selectionModel()->selectedRows(0));
   if (!data) { return; }
   auto *drag = new QDrag(this);
   drag->setMimeData(data);
   drag->exec(Qt::MoveAction);
   emit dragFinished();
}

PersistentTaskOrderView::PersistentTaskOrderView(PersistentTaskOrderTable *table, QListWidget *groups,
                                                 PersistentTaskFilter *proxy, Describe describe)
   : QObject(table), table(table), groups(groups), proxy(proxy), describe(std::move(describe)),
     line(new QFrame(table->viewport())) {
   line->setAttribute(Qt::WA_TransparentForMouseEvents);
   line->setObjectName("taskInsertionLine");
   line->setStyleSheet("background-color: palette(highlight);");
   line->hide();
   table->setDragDropMode(QAbstractItemView::DragDrop);
   table->setDragEnabled(false);
   table->setDropIndicatorShown(false);
   table->setAutoScroll(false);
   groups->setDragDropMode(QAbstractItemView::DropOnly);
   groups->setDropIndicatorShown(false);
   groups->setAutoScroll(false);
   for (auto *view : {static_cast<QAbstractItemView *>(table), static_cast<QAbstractItemView *>(groups)}) {
      view->viewport()->installEventFilter(this);
      view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
   }
   connect(table, &PersistentTaskOrderTable::dragFinished, this, &PersistentTaskOrderView::clearPreview);
   scrollTimer.setInterval(50);
   connect(&scrollTimer, &QTimer::timeout, this, &PersistentTaskOrderView::scroll);
}

void PersistentTaskOrderView::setEnabled(bool value) {
   if (enabled == value) { return; }
   enabled = value;
   table->setDragEnabled(value);
   proxy->setTaskOrderingEnabled(value);
   if (!value) { clearPreview(); }
}

void PersistentTaskOrderView::clearPreview() {
   scrollTimer.stop();
   mime.clear();
   surface.clear();
   displayed.reset();
   line->hide();
   emit previewChanged({});
}

std::optional<PersistentTaskOrderView::Offer> PersistentTaskOrderView::offerAt(QAbstractItemView *view,
                                                                             const QPoint &point,
                                                                             const QMimeData *data) const {
   const auto hit = view->indexAt(point);
   if (!hit.isValid()) { return {}; }
   const auto rect = view->visualRect(hit);
   const bool after = point.y() >= rect.center().y();
   auto preview = view == table ? proxy->previewTableDrop(data, hit, after)
                                : proxy->previewGroupDrop(data, hit.data(Qt::UserRole).toString().toStdString());
   if (!preview) { return {}; }
   return Offer{std::move(*preview), hit, after, view != table || after ? rect.bottom() + 1 : rect.top()};
}

void PersistentTaskOrderView::showOffer(QAbstractItemView *view, const Offer &offer) {
   auto *viewport = view->viewport();
   if (line->parentWidget() != viewport) { line->setParent(viewport); }
   const int y = std::clamp(offer.y - 1, 0, std::max(0, viewport->height() - 3));
   line->setGeometry(0, y, viewport->width(), 3);
   line->show();
   line->raise();
   displayed = offer.preview;
   emit previewChanged(describe(offer.preview));
}

void PersistentTaskOrderView::scroll() {
   if (!surface || !mime) { clearPreview(); return; }
   auto *bar = surface->verticalScrollBar();
   constexpr int EDGE = 24;
   const int direction = position.y() < EDGE ? -1 : position.y() >= surface->viewport()->height() - EDGE ? 1 : 0;
   if (direction == 0) { return; }
   const auto previous = bar->value();
   bar->setValue(previous + direction * EDGE);
   if (bar->value() == previous) { return; }
   if (const auto offer = offerAt(surface, position, mime)) { showOffer(surface, *offer); }
   else { line->hide(); displayed.reset(); emit previewChanged({}); }
}

bool PersistentTaskOrderView::eventFilter(QObject *watched, QEvent *event) {
   const auto type = event->type();
   if (type == QEvent::DragLeave) {
      clearPreview();
      return true;
   }
   if (type != QEvent::DragEnter && type != QEvent::DragMove && type != QEvent::Drop) {
      return QObject::eventFilter(watched, event);
   }
   auto *drop = static_cast<QDropEvent *>(event);
   if (!enabled || drop->source() != table || !(drop->possibleActions() & Qt::MoveAction)) {
      drop->ignore();
      return true;
   }
   auto *view = watched == table->viewport() ? static_cast<QAbstractItemView *>(table) : groups;
   drop->setDropAction(Qt::MoveAction);
   if (type == QEvent::DragEnter) {
      drop->accept();
      return true;
   }
   const auto offer = offerAt(view, drop->position().toPoint(), drop->mimeData());
   if (type == QEvent::Drop) {
      const bool matched = offer && displayed && offer->preview == *displayed;
      clearPreview();
      if (matched) {
         if (view == table) {
            const int row = offer->after ? -1 : offer->hit.row();
            if (!proxy->canDropMimeData(drop->mimeData(), Qt::MoveAction, row, 0, offer->hit)
                || !proxy->dropMimeData(drop->mimeData(), Qt::MoveAction, row, 0, offer->hit)) {
               drop->ignore();
               return true;
            }
         } else {
            proxy->commitTaskMove(offer->preview);
         }
         drop->accept();
      } else { drop->ignore(); }
      return true;
   }
   surface = view;
   mime = drop->mimeData();
   position = drop->position().toPoint();
   if (!scrollTimer.isActive()) { scrollTimer.start(); }
   if (offer) {
      showOffer(view, *offer);
      drop->accept();
   } else {
      line->hide();
      displayed.reset();
      emit previewChanged({});
      drop->ignore();
   }
   return true;
}
