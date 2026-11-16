#pragma once
// V2.1: every icon file under resources/icons/ (embedded via icons.qrc) is a plain monochrome
// line-art SVG -- color comes entirely from this helper, which rasterizes the SVG once via
// QSvgRenderer and then overwrites every opaque pixel with the requested QColor (CompositionMode
// SourceIn), rather than keeping separate SVG variants per color. Rendered at a 2x supersample
// and tagged with that device pixel ratio so icons stay crisp on high-DPI displays.
#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QString>

namespace icons {

// `name` is the .qrc alias without the ":/icons/" prefix or ".svg" suffix, e.g. "play".
QPixmap tintedPixmap(const QString& name, const QColor& color, int pixelSize);
QIcon tintedIcon(const QString& name, const QColor& color, int pixelSize);

} // namespace icons
