#ifndef ULTA_FLASHBACK_EFFECT_H
#define ULTA_FLASHBACK_EFFECT_H

// Flashback look for the viewport: sepia or black and white. Applied as a
// graphics effect on the viewport widget and removed when the flashback ends.

#include <QGraphicsEffect>

class UltaFlashbackEffect : public QGraphicsEffect {
  Q_OBJECT

public:
  enum Look { Sepia, BlackWhite };

  explicit UltaFlashbackEffect(QObject *parent = nullptr);
  void setLook(Look look);
  Look look() const { return m_look; }

protected:
  void draw(QPainter *painter) override;

private:
  Look m_look = Sepia;
};

#endif // ULTA_FLASHBACK_EFFECT_H
