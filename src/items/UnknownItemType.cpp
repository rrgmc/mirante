#include "items/UnknownItemType.h"

#include "items/PaintedIconEngine.h"

#include <QCoreApplication>
#include <QPainter>

namespace mirante {

namespace {

// A dashed outline with a question mark, drawn at any size. Drawn in code
// because mirante_items has no resources of its own.
class UnknownIconEngine : public PaintedIconEngine
{
public:
    void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State) override
    {
        const QColor colour = mode == QIcon::Disabled ? QColor(160, 160, 160) : QColor(110, 110, 110);
        const qreal side = qMin(rect.width(), rect.height());
        const QRectF box(rect.x() + (rect.width() - side) / 2.0,
                         rect.y() + (rect.height() - side) / 2.0, side, side);
        const qreal pen = qMax<qreal>(1.0, side / 16.0);

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(QPen(colour, pen, Qt::DashLine));
        painter->setBrush(Qt::NoBrush);
        const QRectF inner = box.adjusted(pen, pen, -pen, -pen);
        painter->drawRoundedRect(inner, side / 8.0, side / 8.0);

        QFont font = painter->font();
        font.setBold(true);
        font.setPixelSize(qMax(1, int(side * 0.6)));
        painter->setFont(font);
        painter->setPen(colour);
        painter->drawText(inner, Qt::AlignCenter, QStringLiteral("?"));
        painter->restore();
    }

    QIconEngine *clone() const override { return new UnknownIconEngine; }
};

} // namespace

UnknownItemType::UnknownItemType(const QString &id, bool moduleInBuild)
    : m_id(id)
    , m_moduleInBuild(moduleInBuild)
{
}

QString UnknownItemType::id() const { return m_id; }

QString UnknownItemType::name() const
{
    return QCoreApplication::translate("UnknownItemType", "Unknown type %1").arg(m_id);
}

QIcon UnknownItemType::icon() const { return QIcon(new UnknownIconEngine); }

QString UnknownItemType::moduleId() const { return m_id.section(QLatin1Char('.'), 0, 0); }

bool UnknownItemType::moduleInBuild() const { return m_moduleInBuild; }

} // namespace mirante
