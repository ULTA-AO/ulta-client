#include "widgets/ulta_flashback_panel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>

UltaFlashbackPanel::UltaFlashbackPanel(QWidget *parent)
    : UltaDragPanel(parent)
{
  setObjectName("ui_flash_panel");
  setToolTip(tr("Drag the panel to move it."));
  setStyleSheet(QStringLiteral(
      "QFrame#ui_flash_panel { background-color: rgba(20, 24, 40, 235); "
      "border: 1px solid #c8b878; }"
      "QLabel, QCheckBox { color: #f0f0f0; background: transparent; }"
      "QPushButton { color: #101010; background-color: #e4e4e4; padding: 2px 6px; "
      "border: 1px solid #808080; }"
      "QPushButton:hover { background-color: #ffffff; }"
      "QPushButton:disabled { color: #808080; }"
      "QComboBox { color: #101010; background-color: #ffffff; }"
      "QComboBox QAbstractItemView { color: #101010; background-color: #ffffff; "
      "selection-background-color: #3a6ea5; selection-color: #ffffff; }"));

  QGridLayout *layout = new QGridLayout(this);
  layout->setContentsMargins(8, 6, 8, 6);
  layout->setSpacing(4);

  QLabel *title = new QLabel(tr("Flashback"), this);
  QFont f = title->font();
  f.setBold(true);
  title->setFont(f);
  layout->addWidget(title, 0, 0, 1, 2);

  layout->addWidget(new QLabel(tr("Look:"), this), 1, 0);
  m_look = new QComboBox(this);
  m_look->addItem(tr("Sepia"), "s");
  m_look->addItem(tr("Black & white"), "b");
  layout->addWidget(m_look, 1, 1);

  m_hold = new QCheckBox(tr("Hold until I end it"), this);
  m_hold->setToolTip(tr("Other players speaking will not end the flashback."));
  layout->addWidget(m_hold, 2, 0, 1, 2);

  m_button = new QPushButton(tr("Start flashback"), this);
  layout->addWidget(m_button, 3, 0, 1, 2);

  m_status = new QLabel(this);
  m_status->setWordWrap(true);
  layout->addWidget(m_status, 4, 0, 1, 2);

  connect(m_button, &QPushButton::clicked, this, [this]() {
    if (m_active && m_mine)
      emit endRequested();
    else
      emit startRequested(m_look->currentData().toString(),
                          m_hold->isChecked());
  });
  setState(false, false, false);
}

void UltaFlashbackPanel::setState(bool active, bool mine, bool held)
{
  m_active = active;
  m_mine = mine;
  if (!active) {
    m_button->setText(tr("Start flashback"));
    m_button->setEnabled(true);
    m_status->setText(QString());
  }
  else if (mine) {
    m_button->setText(tr("End flashback"));
    m_button->setEnabled(true);
    m_status->setText(held ? tr("Running (held).") : tr("Running."));
  }
  else {
    m_button->setText(tr("Start flashback"));
    m_button->setEnabled(!held);
    m_status->setText(held ? tr("A held flashback is running.")
                           : tr("Someone else's flashback is running."));
  }
  adjustSize();
  clampToParent();
}
