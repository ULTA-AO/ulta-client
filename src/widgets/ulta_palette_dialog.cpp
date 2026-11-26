#include "widgets/ulta_palette_dialog.h"

#include <QComboBox>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

UltaPaletteDialog::UltaPaletteDialog(const QVector<UltaSlotInfo> &slot_infos,
                                     const QVector<QHash<int, QColor>> &presets,
                                     int active_preset, int other_mode,
                                     QWidget *parent)
    : QDialog(parent), m_slots(slot_infos), m_presets(presets),
      m_active(active_preset)
{
  setWindowTitle(tr("Custom text colors"));

  // Don't inherit the app palette: under some setups it gives light text on
  // this dialog's light background. Set readable colors explicitly.
  setStyleSheet(QStringLiteral(
      "UltaPaletteDialog { background-color: #f0f0f0; }"
      "QLabel, QCheckBox { color: #101010; }"
      "QComboBox { color: #101010; background-color: #ffffff; }"
      "QComboBox QAbstractItemView { color: #101010; background-color: "
      "#ffffff; selection-background-color: #3a6ea5; selection-color: "
      "#ffffff; }"));

  // Always exactly PRESET_COUNT hotslots.
  while (m_presets.size() < PRESET_COUNT)
    m_presets.append(QHash<int, QColor>());
  if (m_active < 0 || m_active >= PRESET_COUNT)
    m_active = 0;

  QVBoxLayout *root = new QVBoxLayout(this);

  QLabel *intro = new QLabel(
      tr("Pick your own colors for the text symbols. Other ULTA players see "
         "your messages in the colors you choose here; everyone else sees the "
         "normal colors."),
      this);
  intro->setWordWrap(true);
  root->addWidget(intro);

  // Hotslot row
  QHBoxLayout *preset_row = new QHBoxLayout();
  preset_row->addWidget(new QLabel(tr("Hotslot:"), this));
  for (int i = 0; i < PRESET_COUNT; ++i) {
    QPushButton *b = new QPushButton(QString::number(i + 1), this);
    b->setCheckable(true);
    b->setMinimumWidth(28);
    b->setToolTip(tr("Hotslot %1: a saved set of color choices").arg(i + 1));
    connect(b, &QPushButton::clicked, this, [this, i]() { onPresetClicked(i); });
    preset_row->addWidget(b);
    m_preset_buttons.append(b);
  }
  preset_row->addStretch(1);
  root->addLayout(preset_row);

  // One row per color slot: swatch, label, reset.
  QGridLayout *grid = new QGridLayout();
  for (int row = 0; row < m_slots.size(); ++row) {
    const UltaSlotInfo &info = m_slots.at(row);

    QPushButton *swatch = new QPushButton(this);
    swatch->setMinimumSize(70, 24);
    connect(swatch, &QPushButton::clicked, this,
            [this, row]() { onPickColor(row); });
    m_swatches.append(swatch);

    QLabel *name = new QLabel(info.name, this); // text set in refresh()
    m_names.append(name);

    QPushButton *reset = new QPushButton(tr("Reset"), this);
    connect(reset, &QPushButton::clicked, this,
            [this, row]() { onResetColor(row); });
    m_resets.append(reset);

    grid->addWidget(swatch, row, 0);
    grid->addWidget(name, row, 1);
    grid->addWidget(reset, row, 2);
  }
  grid->setColumnStretch(1, 1);
  root->addLayout(grid);

  QHBoxLayout *others_row = new QHBoxLayout();
  others_row->addWidget(new QLabel(tr("Other players' text colors:"), this));
  m_other_mode = new QComboBox(this);
  m_other_mode->addItem(tr("Their custom colors (default)"), OTHER_THEIRS);
  m_other_mode->addItem(tr("Theme colors"), OTHER_THEME);
  m_other_mode->addItem(tr("My colors (active hotslot)"), OTHER_MINE);
  int mode_index = m_other_mode->findData(other_mode);
  m_other_mode->setCurrentIndex(mode_index >= 0 ? mode_index : 0);
  m_other_mode->setToolTip(
      tr("How other people's messages look on YOUR screen.\n"
         "Their custom colors: what each sender chose (theme colors if they "
         "chose none).\n"
         "Theme colors: ignore everyone's custom colors.\n"
         "My colors: show everyone's text, including stock-client players, "
         "using your active hotslot.\n"
         "Your own messages always use your own colors."));
  others_row->addWidget(m_other_mode, 1);
  root->addLayout(others_row);

  QDialogButtonBox *buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  root->addWidget(buttons);

  refresh();
}

int UltaPaletteDialog::otherMode() const
{
  return m_other_mode->currentData().toInt();
}

QString UltaPaletteDialog::nearestColorName(const QColor &c)
{
  struct Named {
    const char *name;
    int r, g, b;
  };
  static const Named table[] = {
      {QT_TR_NOOP("Black"), 0, 0, 0},
      {QT_TR_NOOP("White"), 255, 255, 255},
      {QT_TR_NOOP("Gray"), 128, 128, 128},
      {QT_TR_NOOP("Silver"), 192, 192, 192},
      {QT_TR_NOOP("Red"), 255, 0, 0},
      {QT_TR_NOOP("Maroon"), 128, 0, 0},
      {QT_TR_NOOP("Orange"), 255, 165, 0},
      {QT_TR_NOOP("Brown"), 139, 69, 19},
      {QT_TR_NOOP("Yellow"), 255, 255, 0},
      {QT_TR_NOOP("Gold"), 218, 165, 32},
      {QT_TR_NOOP("Olive"), 128, 128, 0},
      {QT_TR_NOOP("Lime"), 0, 255, 0},
      {QT_TR_NOOP("Green"), 0, 160, 0},
      {QT_TR_NOOP("Dark green"), 0, 100, 0},
      {QT_TR_NOOP("Teal"), 0, 128, 128},
      {QT_TR_NOOP("Cyan"), 0, 255, 255},
      {QT_TR_NOOP("Sky blue"), 135, 206, 235},
      {QT_TR_NOOP("Blue"), 0, 0, 255},
      {QT_TR_NOOP("Navy"), 0, 0, 128},
      {QT_TR_NOOP("Purple"), 128, 0, 128},
      {QT_TR_NOOP("Violet"), 186, 85, 211},
      {QT_TR_NOOP("Magenta"), 255, 0, 255},
      {QT_TR_NOOP("Pink"), 255, 192, 203},
  };

  // "Redmean" weighted distance: closer to how people see color differences
  // than plain RGB distance.
  const Named *best = &table[0];
  double best_dist = 1e18;
  for (const Named &n : table) {
    double rmean = (c.red() + n.r) / 2.0;
    double dr = c.red() - n.r, dg = c.green() - n.g, db = c.blue() - n.b;
    double dist = (2.0 + rmean / 256.0) * dr * dr + 4.0 * dg * dg +
                  (2.0 + (255.0 - rmean) / 256.0) * db * db;
    if (dist < best_dist) {
      best_dist = dist;
      best = &n;
    }
  }
  return tr(best->name);
}

QString UltaPaletteDialog::slotLabel(const QString &symbol,
                                     const QString &theme_name,
                                     const QColor *override_color)
{
  QString name =
      override_color ? nearestColorName(*override_color) : theme_name;
  return symbol.isEmpty() ? name : QStringLiteral("%1 %2").arg(symbol, name);
}

void UltaPaletteDialog::onPresetClicked(int index)
{
  m_active = index;
  refresh();
}

void UltaPaletteDialog::onPickColor(int row)
{
  const UltaSlotInfo &info = m_slots.at(row);
  QColor current = m_presets[m_active].value(info.slot, info.theme_color);
  QColor chosen = QColorDialog::getColor(
      current, this, tr("Color for %1").arg(info.name));
  if (!chosen.isValid())
    return;
  m_presets[m_active].insert(info.slot, chosen);
  refresh();
}

void UltaPaletteDialog::onResetColor(int row)
{
  m_presets[m_active].remove(m_slots.at(row).slot);
  refresh();
}

void UltaPaletteDialog::refresh()
{
  for (int i = 0; i < m_preset_buttons.size(); ++i)
    m_preset_buttons[i]->setChecked(i == m_active);

  for (int row = 0; row < m_slots.size(); ++row) {
    const UltaSlotInfo &info = m_slots.at(row);
    bool overridden = m_presets.at(m_active).contains(info.slot);
    QColor c = m_presets.at(m_active).value(info.slot, info.theme_color);
    // Pick black or white text so the hex code stays readable on the swatch.
    // qGray weighs green most, so bright greens/yellows/cyans get black text.
    QString fg = qGray(c.rgb()) > 110 ? "#000000" : "#ffffff";
    m_swatches[row]->setText(c.name());
    m_swatches[row]->setStyleSheet(
        QStringLiteral("background-color: %1; color: %2; border: 1px solid "
                       "#404040;")
            .arg(c.name(), fg));
    m_resets[row]->setEnabled(overridden);

    // Label follows the actual color: "# Teal" for an overridden slot, the
    // theme's own name for an untouched one. The theme name stays as tooltip.
    m_names[row]->setText(
        slotLabel(info.symbol, info.name, overridden ? &c : nullptr));
    m_names[row]->setToolTip(tr("Theme name: %1").arg(info.name));
  }
}
