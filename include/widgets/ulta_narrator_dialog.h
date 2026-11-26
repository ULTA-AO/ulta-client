#ifndef ULTA_NARRATOR_DIALOG_H
#define ULTA_NARRATOR_DIALOG_H

// Settings for narrator mode

#include <QDialog>
#include <QString>

class QComboBox;
class QLineEdit;

class UltaNarratorDialog : public QDialog {
  Q_OBJECT

public:
  // scene: 0 = over own character, 1 = alone
  UltaNarratorDialog(int scene, const QString &folder, const QString &name,
                     QWidget *parent = nullptr);

  int scene() const;
  QString folder() const;  // NarratorULTA / NarratorULTAMale / NarratorULTAFemale
  QString name() const;    // sanitized, never empty

  // Strip characters the protocol can't carry, limit the length
  static QString sanitizeName(const QString &name);

private:
  QComboBox *m_scene = nullptr;
  QComboBox *m_voice = nullptr;
  QLineEdit *m_name = nullptr;
};

#endif // ULTA_NARRATOR_DIALOG_H
