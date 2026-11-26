#ifndef AOCLOCKLABEL_H
#define AOCLOCKLABEL_H

#include <QLabel>
#include <QBasicTimer>
#include <QTimerEvent>
#include <QDateTime>
#include <QDebug>

class AOClockLabel : public QLabel {
  Q_OBJECT

public:
  AOClockLabel(QWidget *parent);
  void start();
  void start(qint64 msecs);
  void set(qint64 msecs, bool update_text = false);
  void pause();
  void stop();
  void skip(qint64 msecs);
  bool active();

protected:
  void timerEvent(QTimerEvent *event) override;
  void paintEvent(QPaintEvent *event) override;

private:
    QBasicTimer timer;
    QDateTime target_time;
    // When the countdown reaches zero the border pulses for a few seconds.
    QBasicTimer m_pulse;
    int m_pulse_ticks = 0;
    void start_pulse();
    void stop_pulse();
};

#endif // AOCLOCKLABEL_H
