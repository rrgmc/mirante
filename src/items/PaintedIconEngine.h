#pragma once

#include <QIconEngine>
#include <QPainter>
#include <QPixmap>

namespace mirante {

// The base of icons that are painted in code. QIconEngine's own pixmap() and
// scaledPixmap() paint into a pixmap whose pixels are never cleared, so an
// icon shown as a pixmap, as tool buttons and tab bars do, gets stray pixels
// around the shape. These start from a transparent pixmap at the screen's
// scale. Subclasses implement paint() and clone() only.
class PaintedIconEngine : public QIconEngine
{
public:
    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override
    {
        return scaledPixmap(size, mode, state, 1.0);
    }

    QPixmap scaledPixmap(const QSize &size, QIcon::Mode mode, QIcon::State state, qreal scale) override
    {
        QPixmap pixmap(size * scale);
        pixmap.fill(Qt::transparent);
        {
            QPainter painter(&pixmap);
            painter.scale(scale, scale);
            paint(&painter, QRect(QPoint(0, 0), size), mode, state);
        }
        pixmap.setDevicePixelRatio(scale);
        return pixmap;
    }
};

} // namespace mirante
