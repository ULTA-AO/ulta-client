#ifndef ULTA_FADE_OVERLAY_H
#define ULTA_FADE_OVERLAY_H

// Colored layer over the viewport that fades in and out (blackout, whiteout)

#include <QColor>
#include <QWidget>

class QPropertyAnimation;

class UltaFadeOverlay : public QWidget {
  Q_OBJECT
  Q_PROPERTY(qreal fadeLevel READ fadeLevel WRITE setFadeLevel)

public:
  explicit UltaFadeOverlay(QWidget *parent = nullptr);

  void setFadeColor(const QColor &color);
  QColor fadeColor() const { return m_color; }
  qreal fadeLevel() const { return m_level; }
  // 0 = clear, 1 = fully covered
  void setFadeLevel(qreal level);
  // Animate to `level` over `msecs`, replaces a running fade
  void fadeTo(qreal level, int msecs);
  // Stop and clear at once
  void clearNow();

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  QColor m_color = Qt::black;
  qreal m_level = 0.0;
  QPropertyAnimation *m_anim = nullptr;
};

#endif // ULTA_FADE_OVERLAY_H
