#include "widgets/ulta_fade_overlay.h"

#include <QPainter>
#include <QPropertyAnimation>

UltaFadeOverlay::UltaFadeOverlay(QWidget *parent) : QWidget(parent)
{
  setAttribute(Qt::WA_TransparentForMouseEvents);
  setAttribute(Qt::WA_NoSystemBackground);
  m_anim = new QPropertyAnimation(this, "fadeLevel", this);
  hide();
}

void UltaFadeOverlay::setFadeColor(const QColor &color)
{
  m_color = color;
  update();
}

void UltaFadeOverlay::setFadeLevel(qreal level)
{
  m_level = qBound<qreal>(0.0, level, 1.0);
  setVisible(m_level > 0.0);
  update();
}

void UltaFadeOverlay::fadeTo(qreal level, int msecs)
{
  m_anim->stop();
  if (msecs <= 0 || qFuzzyCompare(m_level + 1.0, level + 1.0)) {
    setFadeLevel(level);
    return;
  }
  m_anim->setDuration(msecs);
  m_anim->setStartValue(m_level);
  m_anim->setEndValue(level);
  setVisible(true);
  m_anim->start();
}

void UltaFadeOverlay::clearNow()
{
  m_anim->stop();
  setFadeLevel(0.0);
}

void UltaFadeOverlay::paintEvent(QPaintEvent *)
{
  if (m_level <= 0.0)
    return;
  QColor c = m_color;
  c.setAlphaF(m_level);
  QPainter painter(this);
  painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
  painter.fillRect(rect(), c);
}
