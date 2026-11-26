#include "widgets/ulta_narrator_dialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

UltaNarratorDialog::UltaNarratorDialog(int scene, const QString &folder,
                                       const QString &name, QWidget *parent)
    : QDialog(parent)
{
  setWindowTitle(tr("Narrator settings"));

  // Same explicit styling as the colors dialog: don't inherit the app palette.
  setStyleSheet(QStringLiteral(
      "UltaNarratorDialog { background-color: #f0f0f0; }"
      "QLabel { color: #101010; }"
      "QComboBox, QLineEdit { color: #101010; background-color: #ffffff; }"
      "QComboBox QAbstractItemView { color: #101010; background-color: "
      "#ffffff; selection-background-color: #3a6ea5; selection-color: "
      "#ffffff; }"));

  QVBoxLayout *root = new QVBoxLayout(this);

  QLabel *intro = new QLabel(
      tr("While Narrator mode is on, your messages are shown as narration. "
         "Right-click the Narrator button to open this window."),
      this);
  intro->setWordWrap(true);
  root->addWidget(intro);

  QFormLayout *form = new QFormLayout();

  m_scene = new QComboBox(this);
  m_scene->addItem(tr("Over my character"), 0);
  m_scene->addItem(tr("Alone (hide characters)"), 1);
  m_scene->setCurrentIndex(scene == 1 ? 1 : 0);
  m_scene->setToolTip(
      tr("Over my character: your character stays on screen while the "
         "narrator speaks.\nAlone: only the background and the narration."));
  form->addRow(tr("Scene:"), m_scene);

  m_voice = new QComboBox(this);
  m_voice->addItem(tr("Neutral"), "NarratorULTA");
  m_voice->addItem(tr("Male"), "NarratorULTAMale");
  m_voice->addItem(tr("Female"), "NarratorULTAFemale");
  int voice_index = m_voice->findData(folder);
  m_voice->setCurrentIndex(voice_index >= 0 ? voice_index : 0);
  m_voice->setToolTip(tr("The narrator's voice (text blips)."));
  form->addRow(tr("Voice:"), m_voice);

  m_name = new QLineEdit(sanitizeName(name), this);
  m_name->setMaxLength(24);
  m_name->setToolTip(tr("The name shown on the chatbox while narrating."));
  form->addRow(tr("Name shown:"), m_name);

  root->addLayout(form);

  QDialogButtonBox *buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  root->addWidget(buttons);
}

int UltaNarratorDialog::scene() const
{
  return m_scene->currentData().toInt();
}

QString UltaNarratorDialog::folder() const
{
  return m_voice->currentData().toString();
}

QString UltaNarratorDialog::name() const
{
  return sanitizeName(m_name->text());
}

QString UltaNarratorDialog::sanitizeName(const QString &name)
{
  // '#', '%', '&', '$' are AO packet delimiters; ';' '<' '>' '|' '\' are
  // used by the narration field format. Remove them, trim, limit, never empty.
  QString out;
  for (const QChar &c : name) {
    if (QString("#%&$;<>|\\").contains(c) || c.isNonCharacter() || c < QChar(' '))
      continue;
    out.append(c);
  }
  out = out.trimmed().left(24);
  return out.isEmpty() ? QStringLiteral("Narrator") : out;
}
