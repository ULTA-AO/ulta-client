#ifndef ULTA_FLASHBACK_PANEL_H
#define ULTA_FLASHBACK_PANEL_H

// Small draggable panel to start and end a flashback.

#include "widgets/ulta_drag_panel.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;

class UltaFlashbackPanel : public UltaDragPanel {
  Q_OBJECT

public:
  explicit UltaFlashbackPanel(QWidget *parent = nullptr);

  // active: running, mine: started by this player, held: held
  void setState(bool active, bool mine, bool held);

signals:
  void startRequested(const QString &look, bool hold);
  void endRequested();

private:
  QComboBox *m_look;
  QCheckBox *m_hold;
  QPushButton *m_button;
  QLabel *m_status;
  bool m_active = false;
  bool m_mine = false;
};

#endif // ULTA_FLASHBACK_PANEL_H
