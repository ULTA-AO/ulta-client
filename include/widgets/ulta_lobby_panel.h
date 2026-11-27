#ifndef ULTA_LOBBY_PANEL_H
#define ULTA_LOBBY_PANEL_H

// Minimal lobby for the locked build, covers the stock lobby form

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
  // Update card above the footer
  void showUpdate(const QString &p_version, const QString &p_notes, bool p_required = false);
  // A required update keeps Join off
  void setJoinLocked(bool p_locked);
  void setUpdateProgress(int p_percent);
  // Same as pressing "Update now"
  void beginUpdate();
  void showUpdateError(const QString &p_text);
  // Message in the update card, hides itself after a while
  void showUpdateMessage(const QString &p_title, const QString &p_text, int p_hide_ms = 6000);
  // Show the Updates button
  void setUpdatesEnabled(bool p_enabled);
  // Red card for a reason the player can't join; the button is optional
  void showNotice(const QString &p_title, const QString &p_text, const QString &p_action = QString(), int p_kind = 0);
  void clearNotice();
  QString noticeText() const;

signals:
  void joinClicked();
  void refreshClicked();
  void updateClicked();
  void checkUpdatesClicked();
  void optionsClicked();
  void aboutClicked();
  void quitClicked();
  void noticeActionClicked(int p_kind);

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
  bool m_notice_mode = false;
  int m_notice_kind = 0;
  void setCardRed(bool p_red);
  QWidget *m_update_card;
  QLabel *m_update_title;
  QLabel *m_update_notes;
  QPushButton *m_update_button;
  QProgressBar *m_update_bar;
  void layoutChildren();
};

#endif // ULTA_LOBBY_PANEL_H
