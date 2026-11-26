#include "widgets/ulta_lobby_panel.h"

#include <QLabel>
#include <QLinearGradient>
#include <QProgressBar>
#include <QPainter>
#include <QPushButton>
#include <QRadialGradient>
#include <QTimer>

static const char *kPanelStyle = R"(
QLabel#ulta_server_name {
  color: #c9ccd3;
}
QLabel#ulta_status {
  color: #9aa0ab;
}
QLabel#ulta_version {
  color: #5d636f;
}
QPushButton#ulta_join {
  color: #1b1405;
  border: none;
  border-radius: 26px;
  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                              stop:0 #f0c653, stop:1 #c18f1d);
}
QPushButton#ulta_join:hover {
  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                              stop:0 #f7d572, stop:1 #d29f27);
}
QPushButton#ulta_join:pressed {
  background: #b88418;
}
QPushButton#ulta_join:disabled {
  color: #7b8190;
  background: #2c313b;
}
QWidget#ulta_update_card {
  background: rgba(224, 180, 65, 0.10);
  border: 1px solid rgba(224, 180, 65, 0.60);
  border-radius: 10px;
}
QWidget#ulta_update_card QLabel { color: #e4e6eb; background: transparent; }
QLabel#ulta_update_title { color: #e0b441; font-weight: bold; }
QWidget#ulta_update_card QPushButton {
  color: #1b1405; border: none; border-radius: 6px; padding: 6px 14px; background: #e0b441;
}
QWidget#ulta_update_card QPushButton:disabled { color: #7b8190; background: #2c313b; }
QProgressBar#ulta_update_bar { background: #0d0f13; border: none; border-radius: 3px; max-height: 6px; }
QProgressBar#ulta_update_bar::chunk { background: #e0b441; border-radius: 3px; }
QPushButton#ulta_small {
  color: #8a909c;
  border: none;
  background: transparent;
  padding: 4px 10px;
}
QPushButton#ulta_small:hover {
  color: #e8e9ed;
}
)";

UltaLobbyPanel::UltaLobbyPanel(const QString &p_server_name, QWidget *parent)
    : QWidget(parent), m_title(QStringLiteral(":/resource/ulta/title.png"))
{
  setObjectName("ulta_lobby_panel");
  setStyleSheet(QString::fromLatin1(kPanelStyle));

  m_server_name = new QLabel(p_server_name, this);
  m_server_name->setObjectName("ulta_server_name");
  m_server_name->setAlignment(Qt::AlignCenter);

  m_join = new QPushButton(tr("Join"), this);
  m_join->setObjectName("ulta_join");
  m_join->setCursor(Qt::PointingHandCursor);
  QFont l_join_font = m_join->font();
  l_join_font.setPointSize(l_join_font.pointSize() + 3);
  l_join_font.setBold(true);
  m_join->setFont(l_join_font);
  m_join->setDefault(true);
  m_join->setEnabled(false);
  connect(m_join, &QPushButton::clicked, this, &UltaLobbyPanel::joinClicked);

  m_status = new QLabel(tr("Connecting..."), this);
  m_status->setObjectName("ulta_status");
  m_status->setAlignment(Qt::AlignCenter);

  m_version = new QLabel(this);
  m_version->setObjectName("ulta_version");

  m_check = new QPushButton(tr("Updates"), this);
  m_check->setToolTip(tr("Check for updates"));
  m_check->hide();
  m_refresh = new QPushButton(tr("Refresh"), this);
  m_options = new QPushButton(tr("Options"), this);
  m_about = new QPushButton(tr("About"), this);
  m_quit = new QPushButton(tr("Quit"), this);
  for (QPushButton *b : {m_check, m_refresh, m_options, m_about, m_quit}) {
    b->setObjectName("ulta_small");
    b->setCursor(Qt::PointingHandCursor);
    b->setFlat(true);
  }
  connect(m_check, &QPushButton::clicked, this,
          &UltaLobbyPanel::checkUpdatesClicked);
  connect(m_refresh, &QPushButton::clicked, this,
          &UltaLobbyPanel::refreshClicked);
  connect(m_options, &QPushButton::clicked, this,
          &UltaLobbyPanel::optionsClicked);
  connect(m_about, &QPushButton::clicked, this, &UltaLobbyPanel::aboutClicked);
  connect(m_quit, &QPushButton::clicked, this, &UltaLobbyPanel::quitClicked);

  // Update card, hidden until a newer release is found
  m_update_card = new QWidget(this);
  m_update_card->setObjectName("ulta_update_card");
  m_update_title = new QLabel(m_update_card);
  m_update_title->setObjectName("ulta_update_title");
  m_update_notes = new QLabel(m_update_card);
  m_update_notes->setWordWrap(true);
  m_update_notes->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  m_update_button = new QPushButton(tr("Update now"), m_update_card);
  m_update_button->setCursor(Qt::PointingHandCursor);
  m_update_bar = new QProgressBar(m_update_card);
  m_update_bar->setObjectName("ulta_update_bar");
  m_update_bar->setTextVisible(false);
  m_update_bar->setRange(0, 100);
  m_update_bar->hide();
  connect(m_update_button, &QPushButton::clicked, this, [this] {
    m_update_button->setEnabled(false);
    m_update_bar->setValue(0);
    m_update_bar->show();
    m_update_notes->setText(tr("Downloading the update..."));
    emit updateClicked();
  });
  m_update_card->hide();

  layoutChildren();
}

void UltaLobbyPanel::showUpdate(const QString &p_version, const QString &p_notes, bool p_required)
{
  m_update_title->setText(p_required ? tr("Update %1 is required").arg(p_version)
                                     : tr("Update %1 is available").arg(p_version));
  setJoinLocked(p_required);
  m_update_notes->setText(p_notes.isEmpty() ? tr("A new version of ULTA AO is ready.") : p_notes);
  m_update_button->setText(tr("Update now"));
  m_update_button->setEnabled(true);
  m_update_button->show();
  m_update_bar->hide();
  m_update_card->show();
  layoutChildren();
  update();
}

void UltaLobbyPanel::setUpdateProgress(int p_percent)
{
  m_update_bar->setValue(qBound(0, p_percent, 100));
}

void UltaLobbyPanel::setUpdatesEnabled(bool p_enabled)
{
  m_updates_enabled = p_enabled;
  m_check->setVisible(p_enabled);
  layoutChildren();
}

void UltaLobbyPanel::showUpdateMessage(const QString &p_title, const QString &p_text, int p_hide_ms)
{
  m_update_title->setText(p_title);
  m_update_notes->setText(p_text);
  m_update_button->hide();
  m_update_bar->hide();
  m_update_card->show();
  layoutChildren();
  QTimer::singleShot(p_hide_ms, this, [this, p_title] {
    // Only if it still shows this message
    if (m_update_title->text() == p_title && !m_update_button->isVisible()) {
      m_update_card->hide();
      layoutChildren();
    }
  });
}

void UltaLobbyPanel::showUpdateError(const QString &p_text)
{
  m_update_bar->hide();
  m_update_notes->setText(p_text);
  m_update_button->setText(tr("Try again"));
  m_update_button->setEnabled(true);
  m_update_button->show();
  m_update_card->show();
  layoutChildren();
}

void UltaLobbyPanel::setStatus(const QString &p_text, State p_state)
{
  QString color = "#9aa0ab";
  if (p_state == Online)
    color = "#5fc77a";
  else if (p_state == Connecting)
    color = "#e0b441";
  m_status->setText(QString("<span style='color:%1;'>&#9679;</span>&nbsp; %2")
                        .arg(color, p_text.toHtmlEscaped()));
}

void UltaLobbyPanel::setJoinEnabled(bool p_enabled)
{
  m_join_wanted = p_enabled;
  m_join->setEnabled(m_join_wanted && !m_join_locked);
}

void UltaLobbyPanel::setJoinLocked(bool p_locked)
{
  m_join_locked = p_locked;
  m_join->setEnabled(m_join_wanted && !m_join_locked);
  m_join->setToolTip(p_locked ? tr("Update ULTA AO first.") : QString());
}

void UltaLobbyPanel::setVersion(const QString &p_text)
{
  m_version->setText(p_text);
}

void UltaLobbyPanel::resizeEvent(QResizeEvent *)
{
  layoutChildren();
}

void UltaLobbyPanel::layoutChildren()
{
  const int w = width();
  const int h = height();

  const int bottom_h = 56;   // version and small buttons
  const int name_h = 22;
  const int join_h = 52;
  const int status_h = 22;
  const int gap1 = 12;       // title to server name
  const int gap2 = 36;       // server name to Join
  const int gap3 = 16;       // Join to status

  // Title gets the remaining height, up to 80% of the width
  const int fixed = name_h + gap1 + gap2 + join_h + gap3 + status_h;
  const int card_h = (m_update_card && m_update_card->isVisible()) ? 108 : 0;
  const int avail = h - bottom_h - 24 - card_h;
  const int max_w = qMin(int(w * 0.8), 620);
  const int max_h = qMax(40, avail - fixed);
  QSize title_size = m_title.size();
  title_size.scale(max_w, max_h, Qt::KeepAspectRatio);

  const int content_h = title_size.height() + fixed;
  int y = qMax(12, (avail - content_h) / 2 + 12);

  m_title_rect = QRect((w - title_size.width()) / 2, y, title_size.width(),
                       title_size.height());
  // Scale once, smooth (drawing it big looks jagged)
  const qreal dpr = devicePixelRatioF();
  m_title_scaled = m_title.scaled(
      QSize(qRound(title_size.width() * dpr), qRound(title_size.height() * dpr)),
      Qt::KeepAspectRatio, Qt::SmoothTransformation);
  m_title_scaled.setDevicePixelRatio(dpr);
  y = m_title_rect.bottom() + gap1;
  m_server_name->setGeometry(0, y, w, name_h);
  y += name_h + gap2;

  const int bw = qMin(260, w - 80);
  m_join->setGeometry((w - bw) / 2, y, bw, join_h);
  y += join_h + gap3;
  m_status->setGeometry(0, y, w, status_h);

  if (m_update_card) {
    const int cw = qMin(440, w - 40);
    m_update_card->setGeometry((w - cw) / 2, h - bottom_h - card_h, cw, card_h - 8);
    m_update_title->setGeometry(14, 8, cw - 28, 20);
    m_update_notes->setGeometry(14, 30, cw - 28 - 110, card_h - 8 - 42);
    m_update_button->setGeometry(cw - 14 - 96, 32, 96, 28);
    m_update_bar->setGeometry(14, card_h - 8 - 14, cw - 28, 6);
  }

  const int by = h - 34;
  m_version->setGeometry(14, by, w / 3, 20);
  // Footer buttons, right to left (Updates only when in use)
  const int sw = 64;
  int right = w - 14;
  for (QPushButton *b : {m_quit, m_about, m_options, m_refresh, m_check}) {
    if (b == m_check && !m_updates_enabled)
      continue;
    right -= sw;
    b->setGeometry(right, by - 2, sw, 24);
  }
  // Repaint, the panel draws the title itself
  update();
}

void UltaLobbyPanel::paintEvent(QPaintEvent *)
{
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.setRenderHint(QPainter::SmoothPixmapTransform);

  QLinearGradient bg(0, 0, 0, height());
  bg.setColorAt(0.0, QColor("#0c0e12"));
  bg.setColorAt(1.0, QColor("#171a21"));
  p.fillRect(rect(), bg);

  // Glow behind the title
  QRadialGradient glow(width() / 2.0, m_title_rect.center().y(),
                       width() * 0.55);
  glow.setColorAt(0.0, QColor(214, 160, 40, 46));
  glow.setColorAt(1.0, QColor(214, 160, 40, 0));
  p.fillRect(rect(), glow);

  if (!m_title_scaled.isNull())
    p.drawPixmap(m_title_rect.topLeft(), m_title_scaled);
}
