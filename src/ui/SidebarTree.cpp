#include "ui/SidebarTree.h"

#include "ui/SidebarDelegate.h"
#include "ui/SidebarModel.h"

#include <QPainter>

namespace mirante {

namespace {

constexpr int inset = 6;
constexpr qreal radius = 5.0;

QColor mix(const QColor &a, const QColor &b, qreal amount)
{
    return QColor::fromRgbF(float(a.redF() + (b.redF() - a.redF()) * amount),
                            float(a.greenF() + (b.greenF() - a.greenF()) * amount),
                            float(a.blueF() + (b.blueF() - a.blueF()) * amount));
}

} // namespace

SidebarTree::SidebarTree(QWidget *parent)
    : QTreeView(parent)
{
    // The window colour behind the rows, which Sidebar sets to its own grey.
    QPalette palette = this->palette();
    for (const auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled})
        palette.setColor(group, QPalette::Base, palette.color(group, QPalette::Window));
    setPalette(palette);
}

void SidebarTree::drawRow(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    // The whole width of the row, indentation included, inset on both sides.
    const QRectF row(inset, option.rect.top() + 1, viewport()->width() - 2 * inset, option.rect.height() - 2);
    const bool selected = selectionModel() && selectionModel()->isSelected(index);
    const auto kind = SidebarModel::Kind(index.data(SidebarModel::KindRole).toInt());
    const auto *delegate = qobject_cast<const SidebarDelegate *>(itemDelegate());

    QColor fill;
    if (selected) {
        fill = option.palette.color(QPalette::Active, QPalette::Highlight);
    } else {
        // Projects get a light tint of the highlight colour, so they stand out
        // between groups and tasks.
        if (kind == SidebarModel::Kind::Project)
            fill = mix(option.palette.color(QPalette::Base), option.palette.color(QPalette::Highlight), 0.07);
        // The row of what the hovered dashboard cell shows.
        if (delegate && delegate->isHoverTinted(index)) {
            const QColor base = fill.isValid() ? fill : option.palette.color(QPalette::Base);
            fill = mix(base, option.palette.color(QPalette::Highlight), 0.22);
        }
    }
    if (fill.isValid()) {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(Qt::NoPen);
        painter->setBrush(fill);
        painter->drawRoundedRect(row, radius, radius);
        painter->restore();
    }

    // The view and the delegate draw the selection with a transparent
    // highlight, so only the text changes colour.
    QStyleOptionViewItem rowOption = option;
    for (const auto group : {QPalette::Active, QPalette::Inactive})
        rowOption.palette.setColor(group, QPalette::Highlight, Qt::transparent);
    QTreeView::drawRow(painter, rowOption, index);
}

} // namespace mirante
