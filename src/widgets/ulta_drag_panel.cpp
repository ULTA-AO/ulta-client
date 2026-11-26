#include "widgets/ulta_drag_panel.h"

#include <QMouseEvent>

UltaDragPanel::UltaDragPanel(QWidget *parent) : QFrame(parent)
{
  setCursor(Qt::SizeAllCursor);
}

void UltaDragPanel::clampToParent()
{
  QWidget *p = parentWidget();
  if (!p)
    return;
  int x = qBound(0, this->x(), qMax(0, p->width() - width()));
  int y = qBound(0, this->y(), qMax(0, p->height() - height()));
  if (x != this->x() || y != this->y())
    move(x, y);
}

void UltaDragPanel::mousePressEvent(QMouseEvent *event)
{
  if (event->button() == Qt::LeftButton) {
    m_dragging = true;
    m_grab_offset = event->pos();
    event->accept();
    return;
  }
  QFrame::mousePressEvent(event);
}

void UltaDragPanel::mouseMoveEvent(QMouseEvent *event)
{
  if (!m_dragging) {
    QFrame::mouseMoveEvent(event);
    return;
  }
  if (QWidget *p = parentWidget()) {
    QPoint target = p->mapFromGlobal(event->globalPos()) - m_grab_offset;
    move(target);
    clampToParent();
  }
  event->accept();
}

void UltaDragPanel::mouseReleaseEvent(QMouseEvent *event)
{
  if (m_dragging && event->button() == Qt::LeftButton) {
    m_dragging = false;
    clampToParent();
    emit dragFinished(pos());
    event->accept();
    return;
  }
  QFrame::mouseReleaseEvent(event);
}
