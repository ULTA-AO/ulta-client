#ifndef ULTA_FIND_BAR_H
#define ULTA_FIND_BAR_H

#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QPair>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>

// Search bar for a read-only log. Highlights every match and steps through them.
class UltaFindBar : public QFrame {
  Q_OBJECT

public:
  UltaFindBar(QTextEdit *target, QWidget *parent = nullptr);

  // Which end of the log holds the newest lines (decides the first match shown)
  void setLogGoesDown(bool down) { m_log_down = down; }

  void openBar();
  void closeBar();

signals:
  void opened();
  void closed();

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
  void onQueryChanged();
  void onContentsChanged();
  void step(int direction);

private:
  void refresh(bool jump);
  void applyHighlight();
  void scrollToCurrent();
  void updateCount();

  QTextEdit *m_target;
  QLineEdit *m_edit;
  QLabel *m_count;
  QToolButton *m_prev;
  QToolButton *m_next;
  QToolButton *m_close;
  QTimer m_refresh_timer;
  QList<QPair<int, int>> m_matches;
  int m_current = -1;
  bool m_log_down = true;
  QFont m_saved_font;
};

#endif // ULTA_FIND_BAR_H
