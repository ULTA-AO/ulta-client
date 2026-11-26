#ifndef ULTA_DRAG_PANEL_H
#define ULTA_DRAG_PANEL_H

// Panel that can be dragged by its background or labels

#include <QFrame>
#include <QPoint>

class UltaDragPanel : public QFrame {
  Q_OBJECT

public:
  explicit UltaDragPanel(QWidget *parent = nullptr);

  // Keep the panel inside its parent
  void clampToParent();

signals:
  // Emitted after a drag, pos is in parent coordinates
  void dragFinished(const QPoint &pos);

protected:
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;

private:
  bool m_dragging = false;
  QPoint m_grab_offset; // where inside the panel the user grabbed it
};

#endif // ULTA_DRAG_PANEL_H
