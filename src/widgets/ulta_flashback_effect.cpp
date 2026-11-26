#include "widgets/ulta_flashback_effect.h"

#include <QImage>
#include <QPainter>
#include <QPixmap>

// Sepia mix (256 = full)
static const int kSepiaMix = 140;
// Sepia darkening (256 = none)
static const int kSepiaDark = 226;

static inline int clamp255(int v)
{
  return v > 255 ? 255 : v;
}

UltaFlashbackEffect::UltaFlashbackEffect(QObject *parent)
    : QGraphicsEffect(parent)
{
}

void UltaFlashbackEffect::setLook(Look look)
{
  m_look = look;
  update();
}

void UltaFlashbackEffect::draw(QPainter *painter)
{
  QPoint offset;
  const QPixmap source =
      sourcePixmap(Qt::LogicalCoordinates, &offset, QGraphicsEffect::NoPad);
  if (source.isNull())
    return;

  QImage img = source.toImage().convertToFormat(QImage::Format_RGB32);
  const int w = img.width();
  const int h = img.height();

  for (int y = 0; y < h; ++y) {
    QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
    for (int x = 0; x < w; ++x) {
      const int r = qRed(line[x]);
      const int g = qGreen(line[x]);
      const int b = qBlue(line[x]);
      int outr, outg, outb;
      if (m_look == BlackWhite) {
        outr = outg = outb = (r * 77 + g * 151 + b * 28) >> 8;
      }
      else {
        outr = clamp255((r * 101 + g * 197 + b * 48) >> 8);
        outg = clamp255((r * 89 + g * 176 + b * 43) >> 8);
        outb = clamp255((r * 70 + g * 137 + b * 34) >> 8);
        // Keep some of the original colors
        outr = (r * (256 - kSepiaMix) + outr * kSepiaMix) >> 8;
        outg = (g * (256 - kSepiaMix) + outg * kSepiaMix) >> 8;
        outb = (b * (256 - kSepiaMix) + outb * kSepiaMix) >> 8;
        outr = (outr * kSepiaDark) >> 8;
        outg = (outg * kSepiaDark) >> 8;
        outb = (outb * kSepiaDark) >> 8;
      }
      line[x] = qRgb(outr, outg, outb);
    }
  }
  painter->drawImage(offset, img);
}
