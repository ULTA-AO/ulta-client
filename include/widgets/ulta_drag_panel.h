#ifndef ULTA_DRAG_PANEL_H
#define ULTA_DRAG_PANEL_H

// ULTA AO2: a panel the user can drag around inside its parent window by
// grabbing any part of it that isn't a control (its background or labels).

#include <QFrame>
#include <QPoint>

class UltaDragPanel : public QFrame {
  Q_OBJECT

public:
  explicit UltaDragPanel(QWidget *parent = nullptr);

  // Keep the panel fully inside its parent (call after resizing / restoring).
  void clampToParent();

signals:
  // Emitted when the user lets go after dragging; pos is in parent coordinates.
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
