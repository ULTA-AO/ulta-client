#include "widgets/ulta_find_bar.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QScrollBar>
#include <QTextCursor>
#include <QTextDocument>

namespace {
const int kMaxMatches = 1000;
}

UltaFindBar::UltaFindBar(QTextEdit *target, QWidget *parent)
    : QFrame(parent), m_target(target)
{
  setObjectName("ui_ulta_find_bar");
  setFrameShape(QFrame::NoFrame);
  // Own style so a courtroom theme can't change how it reads
  setStyleSheet(
      "QFrame#ui_ulta_find_bar { background: #171a21; border: 1px solid #aa8a34; border-radius: 5px; }"
      "QLineEdit { background: #0d0f13; color: #e4e6eb; border: 1px solid #2e3440; border-radius: 3px; padding: 1px 4px; }"
      "QLabel { color: #9aa0ab; background: transparent; }"
      "QToolButton { background: #1e222b; color: #e0b441; border: 1px solid #6e5c2a; border-radius: 3px; min-width: 20px; }"
      "QToolButton:hover { background: #2a2f3a; }");

  m_edit = new QLineEdit(this);
  m_edit->setPlaceholderText(tr("Find in log"));
  m_edit->setClearButtonEnabled(false);
  m_edit->installEventFilter(this);

  m_count = new QLabel(this);
  m_count->setMinimumWidth(62);
  m_count->setAlignment(Qt::AlignCenter);

  m_prev = new QToolButton(this);
  m_prev->setText(QString(QChar(0x25B2)));
  m_prev->setToolTip(tr("Previous match (Shift+Enter)"));
  m_next = new QToolButton(this);
  m_next->setText(QString(QChar(0x25BC)));
  m_next->setToolTip(tr("Next match (Enter)"));
  m_close = new QToolButton(this);
  m_close->setText(QString(QChar(0x00D7)));
  m_close->setToolTip(tr("Close (Esc)"));
  for (QToolButton *b : {m_prev, m_next, m_close})
    b->setFocusPolicy(Qt::NoFocus);

  QHBoxLayout *lay = new QHBoxLayout(this);
  lay->setContentsMargins(5, 3, 5, 3);
  lay->setSpacing(4);
  lay->addWidget(m_edit, 1);
  lay->addWidget(m_count);
  lay->addWidget(m_prev);
  lay->addWidget(m_next);
  lay->addWidget(m_close);

  m_refresh_timer.setSingleShot(true);
  m_refresh_timer.setInterval(150);
  connect(&m_refresh_timer, &QTimer::timeout, this, [this] { refresh(false); });
  connect(m_target->document(), &QTextDocument::contentsChanged, this,
          &UltaFindBar::onContentsChanged);
  connect(m_edit, &QLineEdit::textChanged, this, &UltaFindBar::onQueryChanged);
  connect(m_prev, &QToolButton::clicked, this, [this] { step(-1); });
  connect(m_next, &QToolButton::clicked, this, [this] { step(1); });
  connect(m_close, &QToolButton::clicked, this, &UltaFindBar::closeBar);

  hide();
}

void UltaFindBar::openBar()
{
  // A selection in the log becomes the search text
  QString selected = m_target->textCursor().selectedText().trimmed();
  if (!selected.isEmpty() && !selected.contains(QChar(0x2029)) && selected.length() <= 80)
    m_edit->setText(selected);
  m_saved_font = m_target->font();
  show();
  raise();
  m_edit->setFocus();
  m_edit->selectAll();
  refresh(true);
  emit opened();
}

void UltaFindBar::closeBar()
{
  if (!isVisible())
    return;
  hide();
  m_target->setFont(m_saved_font);
  m_matches.clear();
  m_current = -1;
  m_target->setExtraSelections({});
  emit closed();
}

bool UltaFindBar::eventFilter(QObject *watched, QEvent *event)
{
  if (watched == m_edit && event->type() == QEvent::KeyPress) {
    QKeyEvent *key = static_cast<QKeyEvent *>(event);
    if (key->key() == Qt::Key_Escape) {
      closeBar();
      return true;
    }
    if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
      step(key->modifiers() & Qt::ShiftModifier ? -1 : 1);
      return true;
    }
  }
  return QFrame::eventFilter(watched, event);
}

void UltaFindBar::onQueryChanged()
{
  refresh(true);
}

void UltaFindBar::onContentsChanged()
{
  if (isVisible() && !m_edit->text().isEmpty())
    m_refresh_timer.start();
}

void UltaFindBar::refresh(bool jump)
{
  int old_start = (m_current >= 0 && m_current < m_matches.size()) ? m_matches[m_current].first : -1;
  m_matches.clear();
  m_current = -1;

  const QString query = m_edit->text();
  if (!query.isEmpty()) {
    QTextDocument *doc = m_target->document();
    int pos = 0;
    while (m_matches.size() < kMaxMatches) {
      QTextCursor c = doc->find(query, pos);
      if (c.isNull())
        break;
      m_matches.append(qMakePair(c.selectionStart(), c.selectionEnd()));
      pos = c.selectionEnd();
    }
  }

  if (!m_matches.isEmpty()) {
    if (jump || old_start < 0) {
      // First hit is the newest line
      m_current = m_log_down ? m_matches.size() - 1 : 0;
    }
    else {
      m_current = 0;
      for (int i = 0; i < m_matches.size(); ++i) {
        if (m_matches[i].first >= old_start) {
          m_current = i;
          break;
        }
        m_current = i;
      }
    }
  }

  applyHighlight();
  updateCount();
  if (jump)
    scrollToCurrent();
}

void UltaFindBar::step(int direction)
{
  if (m_matches.isEmpty())
    return;
  m_current = (m_current + direction + m_matches.size()) % m_matches.size();
  applyHighlight();
  updateCount();
  scrollToCurrent();
}

void UltaFindBar::applyHighlight()
{
  QList<QTextEdit::ExtraSelection> list;
  QTextDocument *doc = m_target->document();
  for (int i = 0; i < m_matches.size(); ++i) {
    QTextEdit::ExtraSelection sel;
    QTextCursor c(doc);
    c.setPosition(m_matches[i].first);
    c.setPosition(m_matches[i].second, QTextCursor::KeepAnchor);
    sel.cursor = c;
    if (i == m_current) {
      sel.format.setBackground(QColor(224, 180, 65));
      sel.format.setForeground(QColor(27, 20, 5));
    }
    else {
      sel.format.setBackground(QColor(224, 180, 65, 90));
    }
    list.append(sel);
  }
  m_target->setExtraSelections(list);
}

void UltaFindBar::scrollToCurrent()
{
  if (m_current < 0 || m_current >= m_matches.size())
    return;
  QTextCursor c(m_target->document());
  c.setPosition(m_matches[m_current].first);
  QRect r = m_target->cursorRect(c);
  QScrollBar *sb = m_target->verticalScrollBar();
  sb->setValue(sb->value() + r.center().y() - m_target->viewport()->height() / 2);
}

void UltaFindBar::updateCount()
{
  if (m_edit->text().isEmpty()) {
    m_count->setText(QString());
    return;
  }
  if (m_matches.isEmpty()) {
    m_count->setText(tr("No results"));
    return;
  }
  QString total = QString::number(m_matches.size());
  if (m_matches.size() >= kMaxMatches)
    total += "+";
  m_count->setText(QString("%1 / %2").arg(m_current + 1).arg(total));
}
