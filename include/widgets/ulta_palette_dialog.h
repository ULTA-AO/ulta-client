#ifndef ULTA_PALETTE_DIALOG_H
#define ULTA_PALETTE_DIALOG_H

// ULTA AO2: dialog for choosing custom text colors. Each IC color slot (the
// ones behind markup symbols like # or `) can be overridden with any color;
// up to PRESET_COUNT "hotslots" store whole sets of overrides.

#include <QColor>
#include <QDialog>
#include <QHash>
#include <QString>
#include <QVector>

class QComboBox;
class QLabel;
class QPushButton;

struct UltaSlotInfo {
  int slot = 0;        // color slot number (c0..c11)
  QString symbol;      // markup symbol typed in IC (may be empty)
  QString name;        // display name from the theme / chat config
  QColor theme_color;  // the color this slot has when not overridden
};

class UltaPaletteDialog : public QDialog {
  Q_OBJECT

public:
  static const int PRESET_COUNT = 8;

  UltaPaletteDialog(const QVector<UltaSlotInfo> &slot_infos,
                    const QVector<QHash<int, QColor>> &presets,
                    int active_preset, int other_mode,
                    QWidget *parent = nullptr);

  // How other players' text is colored on this screen.
  enum OtherColorsMode {
    OTHER_THEIRS = 0, // their custom colors (theme colors where they have none)
    OTHER_THEME = 1,  // always the theme's colors
    OTHER_MINE = 2    // the active hotslot applied to everyone's symbols
  };

  // Name of the closest basic color ("Teal", "Brown", ...), for labels.
  static QString nearestColorName(const QColor &c);
  // Label for a color slot: "<symbol> <name>". Untouched slots keep their
  // theme name; overridden slots are named after their actual color.
  static QString slotLabel(const QString &symbol, const QString &theme_name,
                           const QColor *override_color);

  QVector<QHash<int, QColor>> presets() const { return m_presets; }
  int activePreset() const { return m_active; }
  int otherMode() const;

private slots:
  void onPresetClicked(int index);
  void onPickColor(int row);
  void onResetColor(int row);

private:
  void refresh();

  QVector<UltaSlotInfo> m_slots;
  QVector<QHash<int, QColor>> m_presets;
  int m_active = 0;

  QVector<QPushButton *> m_preset_buttons;
  QVector<QPushButton *> m_swatches;
  QVector<QPushButton *> m_resets;
  QVector<QLabel *> m_names;
  QComboBox *m_other_mode = nullptr;
};

#endif // ULTA_PALETTE_DIALOG_H
