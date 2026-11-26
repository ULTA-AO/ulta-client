#include "aoclocklabel.h"

#include <QPainter>
#include <cmath>

AOClockLabel::AOClockLabel(QWidget *parent) : QLabel(parent) {}

void AOClockLabel::start()
{
  stop_pulse();
  timer.start(1000 / 60, this);
}

void AOClockLabel::start(qint64 msecs)
{
  this->set(msecs);
  this->start();
}

void AOClockLabel::set(qint64 msecs, bool update_text)
{
  target_time = QDateTime::currentDateTime().addMSecs(msecs);
  if (update_text)
  {
    if (QDateTime::currentDateTime() >= target_time)
    {
      this->setText("00:00:00.000");
    }
    else
    {
      qint64 ms_left = QDateTime::currentDateTime().msecsTo(target_time);
      QTime timeleft = QTime(0, 0).addMSecs(ms_left % (1000 * 3600 * 24));
      QString timestring = timeleft.toString("hh:mm:ss.zzz");
      this->setText(timestring);
    }
  }
}

void AOClockLabel::pause()
{
  timer.stop();
}

void AOClockLabel::stop()
{
  this->setText("00:00:00.000");
  timer.stop();
  stop_pulse();
}

void AOClockLabel::start_pulse()
{
  m_pulse_ticks = 0;
  m_pulse.start(60, this);
  update();
}

void AOClockLabel::stop_pulse()
{
  if (m_pulse.isActive()) {
    m_pulse.stop();
    update();
  }
}

void AOClockLabel::paintEvent(QPaintEvent *event)
{
  QLabel::paintEvent(event);
  if (!m_pulse.isActive())
    return;
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  const double t = (std::sin(m_pulse_ticks * 0.33) + 1.0) / 2.0; // about 1.1 s per pulse
  QColor color(224, 180, 65);
  color.setAlphaF(0.30 + 0.65 * t);
  painter.setPen(QPen(color, 2));
  painter.setBrush(Qt::NoBrush);
  painter.drawRoundedRect(QRectF(rect()).adjusted(1, 1, -1, -1), 4, 4);
}

void AOClockLabel::skip(qint64 msecs)
{
  qint64 ms_left = QDateTime::currentDateTime().msecsTo(target_time);
  this->set(ms_left - msecs, true);
}

bool AOClockLabel::active()
{
  return timer.isActive();
}

void AOClockLabel::timerEvent(QTimerEvent *event)
{
  if (event->timerId() == timer.timerId()) {
    if (QDateTime::currentDateTime() >= target_time)
    {
      this->stop();
      start_pulse(); // the countdown reached zero
      return;
    }
    qint64 ms_left = QDateTime::currentDateTime().msecsTo(target_time);
    QTime timeleft = QTime(0, 0).addMSecs(ms_left % (1000 * 3600 * 24));
    QString timestring = timeleft.toString("hh:mm:ss.zzz");
    this->setText(timestring);
  } else if (event->timerId() == m_pulse.timerId()) {
    if (++m_pulse_ticks > 100) // about 6 seconds, then the border settles
      stop_pulse();
    else
      update();
  } else {
    QWidget::timerEvent(event);
  }
}
