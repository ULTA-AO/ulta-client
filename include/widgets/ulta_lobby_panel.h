#ifndef ULTA_LOBBY_PANEL_H
#define ULTA_LOBBY_PANEL_H

// Minimal lobby for the locked build: title art, one Join button, a status
// line and a few small buttons. It covers the stock lobby form.

#include <QPixmap>
#include <QWidget>

class QLabel;
class QProgressBar;
class QPushButton;

class UltaLobbyPanel : public QWidget {
  Q_OBJECT

public:
  enum State { Offline, Connecting, Online };

  explicit UltaLobbyPanel(const QString &p_server_name, QWidget *parent);
  void setStatus(const QString &p_text, State p_state);
  void setJoinEnabled(bool p_enabled);
  void setVersion(const QString &p_text);
  // The "update available" card above the footer.
  void showUpdate(const QString &p_version, const QString &p_notes, bool p_required = false);
  // A required update keeps the Join button off until the client is updated.
  void setJoinLocked(bool p_locked);
  void setUpdateProgress(int p_percent);
  void showUpdateError(const QString &p_text);
  // A message in the update card (no button); it hides itself after a while.
  void showUpdateMessage(const QString &p_title, const QString &p_text, int p_hide_ms = 6000);
  // Show the "Updates" button (only when this client can check for updates).
  void setUpdatesEnabled(bool p_enabled);

signals:
  void joinClicked();
  void refreshClicked();
  void updateClicked();
  void checkUpdatesClicked();
  void optionsClicked();
  void aboutClicked();
  void quitClicked();

protected:
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private:
  QPixmap m_title;
  QPixmap m_title_scaled; // m_title scaled once to the size it is drawn at
  QLabel *m_server_name;
  QLabel *m_status;
  QLabel *m_version;
  QPushButton *m_join;
  QPushButton *m_check;
  QPushButton *m_refresh;
  QPushButton *m_options;
  QPushButton *m_about;
  QPushButton *m_quit;
  QRect m_title_rect;
  bool m_updates_enabled = false;
  bool m_join_wanted = false;
  bool m_join_locked = false;
  QWidget *m_update_card;
  QLabel *m_update_title;
  QLabel *m_update_notes;
  QPushButton *m_update_button;
  QProgressBar *m_update_bar;
  void layoutChildren();
};

#endif // ULTA_LOBBY_PANEL_H
