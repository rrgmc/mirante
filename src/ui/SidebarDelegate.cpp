#include "ui/SidebarDelegate.h"

#include "core/Entities.h"
#include "ui/SidebarModel.h"

#include <QIconEngine>
#include <QPainter>

namespace mirante {

namespace {

// Opacity of Low projects and tasks, as in the prototype.
constexpr qreal dimmedOpacity = 0.5;

// Corner radius of the project marker, as a share of its side.
constexpr qreal squareRadius = 0.25;

// A filled dot or rounded square, half the size of the icon, drawn at any size.
class MarkerIconEngine : public QIconEngine
{
public:
    MarkerIconEngine(const QColor &colour, SidebarDelegate::MarkerShape shape)
        : m_colour(colour)
        , m_shape(shape)
    {
    }

    void paint(QPainter *painter, const QRect &rect, QIcon::Mode, QIcon::State) override
    {
        if (m_colour.alpha() == 0)
            return;
        const qreal side = qMin(rect.width(), rect.height()) / 2.0;
        const QRectF dot(rect.x() + (rect.width() - side) / 2.0, rect.y() + (rect.height() - side) / 2.0,
                         side, side);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(Qt::NoPen);
        painter->setBrush(m_colour);
        if (m_shape == SidebarDelegate::MarkerShape::RoundedSquare)
            painter->drawRoundedRect(dot, side * squareRadius, side * squareRadius);
        else
            painter->drawEllipse(dot);
        painter->restore();
    }

    QIconEngine *clone() const override { return new MarkerIconEngine(m_colour, m_shape); }

private:
    QColor m_colour;
    SidebarDelegate::MarkerShape m_shape;
};

bool isDimmed(const QModelIndex &index)
{
    const auto kind = SidebarModel::Kind(index.data(SidebarModel::KindRole).toInt());
    if (kind != SidebarModel::Kind::Project && kind != SidebarModel::Kind::Task)
        return false;
    return index.data(SidebarModel::PriorityRole).toInt() == int(Priority::Low);
}

} // namespace

// The colours of the prototype. Normal is faint, so that Urgent and High stand out.
SidebarDelegate::SidebarDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
    QColor normal(0x3b, 0x82, 0xf6);
    normal.setAlphaF(0.35f);
    QColor colours[4];
    colours[int(Priority::Urgent)] = QColor(0xd9, 0x3b, 0x3b);
    colours[int(Priority::High)] = QColor(0xe8, 0x86, 0x2a);
    colours[int(Priority::Normal)] = normal;
    colours[int(Priority::Low)] = QColor(0x9a, 0xa1, 0xad);
    for (int priority = 0; priority < 4; ++priority) {
        m_projectMarkers[priority] = QIcon(new MarkerIconEngine(colours[priority], MarkerShape::RoundedSquare));
        m_taskMarkers[priority] = QIcon(new MarkerIconEngine(colours[priority], MarkerShape::Circle));
    }
}

// Projects get a rounded square and tasks a circle, so the two levels are
// easy to tell apart.
SidebarDelegate::MarkerShape SidebarDelegate::markerShape(SidebarModel::Kind kind)
{
    return kind == SidebarModel::Kind::Project ? MarkerShape::RoundedSquare : MarkerShape::Circle;
}

void SidebarDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    // Row backgrounds, the selection included, are painted by SidebarTree.
    if (!isDimmed(index)) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }
    painter->save();
    painter->setOpacity(dimmedOpacity);
    QStyledItemDelegate::paint(painter, option, index);
    painter->restore();
}

// Rows a little taller than the default, as in macOS sidebars.
QSize SidebarDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    return QStyledItemDelegate::sizeHint(option, index) + QSize(0, 4);
}

void SidebarDelegate::setHoverId(const QString &id) { m_hoverId = id; }

QString SidebarDelegate::hoverId() const { return m_hoverId; }

// Tasks and items, which dashboard cells show, and the groups and projects
// that stand in for them while collapsed. Not the Dashboards and Items rows.
bool SidebarDelegate::isHoverTinted(const QModelIndex &index) const
{
    if (m_hoverId.isEmpty() || index.data(SidebarModel::IdRole).toString() != m_hoverId)
        return false;
    const auto kind = SidebarModel::Kind(index.data(SidebarModel::KindRole).toInt());
    return kind == SidebarModel::Kind::Task || kind == SidebarModel::Kind::Item || kind == SidebarModel::Kind::Project
        || kind == SidebarModel::Kind::Group;
}

// The marker takes the place of the icon, which projects and tasks do not have.
void SidebarDelegate::initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const
{
    QStyledItemDelegate::initStyleOption(option, index);
    const auto kind = SidebarModel::Kind(index.data(SidebarModel::KindRole).toInt());
    if (kind != SidebarModel::Kind::Project && kind != SidebarModel::Kind::Task)
        return;

    const int priority = index.data(SidebarModel::PriorityRole).toInt();
    if (priority < 0 || priority >= 4)
        return;
    option->icon = markerShape(kind) == MarkerShape::RoundedSquare ? m_projectMarkers[priority]
                                                                   : m_taskMarkers[priority];
    option->features |= QStyleOptionViewItem::HasDecoration;
}

} // namespace mirante
