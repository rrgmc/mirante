#include "ui/SidebarDelegate.h"

#include "core/Entities.h"
#include "ui/SidebarModel.h"

#include <QIconEngine>
#include <QPainter>

namespace mirante {

namespace {

// Opacity of Low projects and tasks, as in the prototype.
constexpr qreal dimmedOpacity = 0.5;

// A filled dot, half the size of the icon, drawn at any size.
class MarkerIconEngine : public QIconEngine
{
public:
    explicit MarkerIconEngine(const QColor &colour)
        : m_colour(colour)
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
        painter->drawEllipse(dot);
        painter->restore();
    }

    QIconEngine *clone() const override { return new MarkerIconEngine(m_colour); }

private:
    QColor m_colour;
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
    m_markers[int(Priority::Urgent)] = QIcon(new MarkerIconEngine(QColor(0xd9, 0x3b, 0x3b)));
    m_markers[int(Priority::High)] = QIcon(new MarkerIconEngine(QColor(0xe8, 0x86, 0x2a)));
    m_markers[int(Priority::Normal)] = QIcon(new MarkerIconEngine(normal));
    m_markers[int(Priority::Low)] = QIcon(new MarkerIconEngine(QColor(0x9a, 0xa1, 0xad)));
}

void SidebarDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    // Projects get a light tint of the highlight colour, so they stand out
    // between groups and tasks.
    if (SidebarModel::Kind(index.data(SidebarModel::KindRole).toInt()) == SidebarModel::Kind::Project) {
        const QColor base = option.palette.color(QPalette::Base);
        const QColor tint = option.palette.color(QPalette::Highlight);
        const qreal amount = 0.12;
        painter->fillRect(option.rect, QColor::fromRgbF(float(base.redF() + (tint.redF() - base.redF()) * amount),
                                                        float(base.greenF() + (tint.greenF() - base.greenF()) * amount),
                                                        float(base.blueF() + (tint.blueF() - base.blueF()) * amount)));
    }
    // The row of what the hovered dashboard cell shows.
    if (isHoverTinted(index)) {
        QColor tint = option.palette.color(QPalette::Highlight);
        tint.setAlphaF(0.22f);
        painter->fillRect(option.rect, tint);
    }
    if (!isDimmed(index)) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }
    painter->save();
    painter->setOpacity(dimmedOpacity);
    QStyledItemDelegate::paint(painter, option, index);
    painter->restore();
}

void SidebarDelegate::setHoverId(const QString &id) { m_hoverId = id; }

QString SidebarDelegate::hoverId() const { return m_hoverId; }

// Only tasks and items: they are what dashboard cells show.
bool SidebarDelegate::isHoverTinted(const QModelIndex &index) const
{
    if (m_hoverId.isEmpty() || index.data(SidebarModel::IdRole).toString() != m_hoverId)
        return false;
    const auto kind = SidebarModel::Kind(index.data(SidebarModel::KindRole).toInt());
    return kind == SidebarModel::Kind::Task || kind == SidebarModel::Kind::Item;
}

// The marker takes the place of the icon, which projects and tasks do not have.
void SidebarDelegate::initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const
{
    QStyledItemDelegate::initStyleOption(option, index);
    const auto kind = SidebarModel::Kind(index.data(SidebarModel::KindRole).toInt());
    if (kind != SidebarModel::Kind::Project && kind != SidebarModel::Kind::Task)
        return;

    const int priority = index.data(SidebarModel::PriorityRole).toInt();
    if (priority >= 0 && priority < 4)
        option->icon = m_markers[priority];
    else
        return;
    option->features |= QStyleOptionViewItem::HasDecoration;
}

} // namespace mirante
