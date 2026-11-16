#include "IconProvider.h"

#include <QPainter>
#include <QSvgRenderer>

namespace icons {

QPixmap tintedPixmap(const QString& name, const QColor& color, int pixelSize) {
  constexpr int kSupersample = 2; // crisp on high-DPI screens; see header comment
  const int renderSize = pixelSize * kSupersample;

  QPixmap pixmap(renderSize, renderSize);
  pixmap.fill(Qt::transparent);

  QSvgRenderer renderer(QStringLiteral(":/icons/%1.svg").arg(name));
  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing);
  renderer.render(&painter, QRectF(0, 0, renderSize, renderSize));

  painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
  painter.fillRect(pixmap.rect(), color);
  painter.end();

  pixmap.setDevicePixelRatio(kSupersample);
  return pixmap;
}

QIcon tintedIcon(const QString& name, const QColor& color, int pixelSize) {
  return QIcon(tintedPixmap(name, color, pixelSize));
}

} // namespace icons
