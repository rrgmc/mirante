#include "ui/SidebarDelegate.h"

#include "core/Entities.h"
#include "items/PaintedIconEngine.h"
#include "ui/SidebarModel.h"

#include <QApplication>
#include <QPainter>
#include <QStyle>

namespace mirante {

namespace {

// Opacity of Low projects and tasks, as in the prototype.
constexpr qreal dimmedOpacity = 0.5;

// Corner radius of the project marker, as a share of its side.
constexpr qreal squareRadius = 0.25;

// A filled dot or rounded square, half the size of the icon, drawn at any size.
class MarkerIconEngine : public PaintedIconEngine
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

// The attention badge: white text on a strong red, as in cell headers
// (CellFrame.cpp).
const QColor attentionColour(0xd9, 0x48, 0x3b);

// The space between the badge and the text, and between the badge and the
// right edge of the row.
constexpr int badgeGap = 4;

// The attention count of a row, or 0.
int attentionCount(const QModelIndex &index)
{
    return index.data(SidebarModel::AttentionRole).toInt();
}

// The badge's font: the view's font, bold and a little smaller. It does not
// follow the row's font, so group headings get the same badge as tasks.
QFont badgeFont(const QStyleOptionViewItem &option)
{
    QFont font = option.widget ? option.widget->font() : QApplication::font();
    font.setBold(true);
    font.setPointSizeF(font.pointSizeF() * 0.85);
    return font;
}

QString badgeText(int count) { return count > 99 ? QStringLiteral("99+") : QString::number(count); }

// The badge's place at the right of rect, centred on it vertically.
QRect badgeRect(const QStyleOptionViewItem &option, const QRect &rect, int count)
{
    const QFontMetrics metrics(badgeFont(option));
    const int height = metrics.height() + 2;
    const int width = qMax(height, metrics.horizontalAdvance(badgeText(count)) + height / 2 + 2);
    return QRect(rect.right() - badgeGap - width + 1, rect.center().y() - height / 2, width, height);
}

// The width a badge takes from the row, gaps included, or 0 without one.
int badgeSpace(const QStyleOptionViewItem &option, const QRect &rect, int count)
{
    return count > 0 ? badgeRect(option, rect, count).width() + 2 * badgeGap : 0;
}

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
    // The text keeps clear of the badge, as initStyleOption() narrows the row.
    if (SidebarModel::Kind(index.data(SidebarModel::KindRole).toInt()) == SidebarModel::Kind::Group) {
        paintGroup(painter, option, index);
    } else if (!isDimmed(index)) {
        QStyledItemDelegate::paint(painter, option, index);
    } else {
        painter->save();
        painter->setOpacity(dimmedOpacity);
        QStyledItemDelegate::paint(painter, option, index);
        painter->restore();
    }

    // The attention badge: the count in a round badge at the right of the
    // row. It is never dimmed, since it asks for action now.
    const int count = attentionCount(index);
    if (count <= 0)
        return;
    const QRect badge = badgeRect(option, option.rect, count);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(Qt::NoPen);
    painter->setBrush(attentionColour);
    painter->drawRoundedRect(QRectF(badge), badge.height() / 2.0, badge.height() / 2.0);
    painter->setFont(badgeFont(option));
    painter->setPen(Qt::white);
    painter->drawText(badge, Qt::AlignCenter, badgeText(count));
    painter->restore();
}

// A group's name in a pill of the group's colour: a light tint of it behind
// the name, and a thin edge in the colour itself. On the selected row the
// selection shows instead, and the name takes the selection's text colour.
// See docs/decisions/0046-group-colours.md.
void SidebarDelegate::paintGroup(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    const QWidget *widget = opt.widget;
    QStyle *style = widget ? widget->style() : QApplication::style();
    const QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, widget);
    const QFontMetrics metrics(opt.font);
    const int padding = metrics.height() / 2;
    const QString text = metrics.elidedText(opt.text, Qt::ElideRight, textRect.width() - 2 * padding);
    const qreal height = metrics.height() + 4;
    const QRectF pill(textRect.left(), textRect.center().y() + 0.5 - height / 2,
                      metrics.horizontalAdvance(text) + 2 * padding, height);

    const bool selected = opt.state & QStyle::State_Selected;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    const QColor colour = index.data(SidebarModel::GroupColourRole).value<QColor>();
    if (!selected && colour.isValid()) {
        const QColor base = opt.palette.color(QPalette::Base);
        const auto mix = [](const QColor &a, const QColor &b, qreal amount) {
            return QColor::fromRgbF(float(a.redF() + (b.redF() - a.redF()) * amount),
                                    float(a.greenF() + (b.greenF() - a.greenF()) * amount),
                                    float(a.blueF() + (b.blueF() - a.blueF()) * amount));
        };
        painter->setPen(QPen(mix(base, colour, 0.6), 1));
        painter->setBrush(mix(base, colour, 0.28));
        painter->drawRoundedRect(pill.adjusted(0.5, 0.5, -0.5, -0.5), height / 2, height / 2);
    }
    painter->setFont(opt.font);
    painter->setPen(opt.palette.color(selected ? QPalette::HighlightedText : QPalette::Text));
    painter->drawText(pill.adjusted(padding, 0, -padding, 0), Qt::AlignVCenter | Qt::AlignLeft, text);
    painter->restore();
}

// Rows a little taller than the default, as in macOS sidebars, and group
// headings taller still, for their pill.
QSize SidebarDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    const bool group = SidebarModel::Kind(index.data(SidebarModel::KindRole).toInt()) == SidebarModel::Kind::Group;
    return QStyledItemDelegate::sizeHint(option, index)
        + QSize(badgeSpace(option, option.rect, attentionCount(index)), group ? 10 : 4);
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
    // A row with an attention badge leaves the badge's space at its right, so
    // the text, and anything painted after it, is cut before the badge.
    if (const int space = badgeSpace(*option, option->rect, attentionCount(index)); space > 0)
        option->rect.setRight(qMax(option->rect.left(), option->rect.right() - space));
    // On the selected row's highlight fill, text in the highlighted text
    // colour. The Windows 11 style draws selected text in the normal text
    // colour, which is hard to read on the full highlight.
    if (option->state & QStyle::State_Selected) {
        for (const auto group : {QPalette::Active, QPalette::Inactive}) {
            const QColor text = option->palette.color(QPalette::Active, QPalette::HighlightedText);
            option->palette.setColor(group, QPalette::Text, text);
            option->palette.setColor(group, QPalette::WindowText, text);
        }
    }
    const auto kind = SidebarModel::Kind(index.data(SidebarModel::KindRole).toInt());
    // A group heading has its name in a pill instead of a marker; see paintGroup().
    if (kind == SidebarModel::Kind::Group)
        return;
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
