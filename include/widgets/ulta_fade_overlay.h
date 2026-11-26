#ifndef ULTA_FADE_OVERLAY_H
#define ULTA_FADE_OVERLAY_H

// ULTA AO2: a colored layer laid over the viewport scene that can fade in and
// out (blackout / whiteout). It sits above the characters and below the
// chatbox, passes mouse clicks through, and hides itself when fully clear.

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
  // 0 = invisible, 1 = fully covered. Shows/hides the widget as needed.
  void setFadeLevel(qreal level);
  // Animate from the current level to `level` over `msecs` (replaces any fade
  // already running).
  void fadeTo(qreal level, int msecs);
  // Stop any animation and clear immediately.
  void clearNow();

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  QColor m_color = Qt::black;
  qreal m_level = 0.0;
  QPropertyAnimation *m_anim = nullptr;
};

#endif // ULTA_FADE_OVERLAY_H
