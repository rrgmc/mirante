#include <mirante/widgets/FlowRow.h>

#include <algorithm>
#include <utility>

namespace mirante::widgets {

namespace {

constexpr int fallbackSpacing = 6;

// Walks the items row by row within rect, and returns the height they take. An
// item wider than the rect gets the rect's width, so a row never needs more
// room than it has. Items are placed only if place is true.
int layOut(const QList<QLayoutItem *> &items, const QRect &rect, int spacing, bool place)
{
    QList<std::pair<QLayoutItem *, QSize>> row;
    int y = rect.y();
    int x = rect.x();
    int rowHeight = 0;
    bool anyRow = false;

    const auto finishRow = [&] {
        if (place) {
            int left = rect.x();
            for (const auto &[item, size] : row) {
                item->setGeometry(QRect(left, y + (rowHeight - size.height()) / 2, size.width(),
                                        size.height()));
                left += size.width() + spacing;
            }
        }
        y += rowHeight + spacing;
        x = rect.x();
        rowHeight = 0;
        row.clear();
        anyRow = true;
    };

    const int available = std::max(rect.width(), 0);
    for (QLayoutItem *item : items) {
        if (item->isEmpty())
            continue;
        const QSize hint = item->sizeHint();
        const int width = std::min(hint.width(), available);
        const int height = item->hasHeightForWidth() ? item->heightForWidth(width) : hint.height();
        if (!row.isEmpty() && x + width > rect.x() + available)
            finishRow();
        row.append({item, QSize(width, height)});
        x += width + spacing;
        rowHeight = std::max(rowHeight, height);
    }
    if (!row.isEmpty())
        finishRow();

    return anyRow ? y - spacing - rect.y() : 0;
}

int effectiveSpacing(const QLayout &layout)
{
    const int spacing = layout.spacing();
    return spacing >= 0 ? spacing : fallbackSpacing;
}

} // namespace

FlowRow::FlowRow(QWidget *parent)
    : QLayout(parent)
{
    setContentsMargins(0, 0, 0, 0);
}

FlowRow::~FlowRow()
{
    while (QLayoutItem *item = takeAt(0))
        delete item;
}

void FlowRow::addItem(QLayoutItem *item)
{
    m_items.append(item);
    invalidate();
}

int FlowRow::count() const { return int(m_items.size()); }

QLayoutItem *FlowRow::itemAt(int index) const { return m_items.value(index); }

QLayoutItem *FlowRow::takeAt(int index)
{
    if (index < 0 || index >= m_items.size())
        return nullptr;
    QLayoutItem *item = m_items.takeAt(index);
    invalidate();
    return item;
}

// The size of everything in one row.
QSize FlowRow::sizeHint() const
{
    const int spacing = effectiveSpacing(*this);
    int width = 0;
    int height = 0;
    bool first = true;
    for (const QLayoutItem *item : m_items) {
        if (item->isEmpty())
            continue;
        const QSize hint = item->sizeHint();
        width += hint.width() + (first ? 0 : spacing);
        height = std::max(height, hint.height());
        first = false;
    }
    const QMargins margins = contentsMargins();
    return QSize(width + margins.left() + margins.right(),
                 height + margins.top() + margins.bottom());
}

// No minimum width, since the row wraps. Otherwise a long button would stop a
// cell from getting narrower, and an action row from reaching its short labels.
QSize FlowRow::minimumSize() const
{
    int height = 0;
    for (const QLayoutItem *item : m_items) {
        if (!item->isEmpty())
            height = std::max(height, item->minimumSize().height());
    }
    const QMargins margins = contentsMargins();
    return QSize(margins.left() + margins.right(), height + margins.top() + margins.bottom());
}

void FlowRow::setGeometry(const QRect &rect)
{
    QLayout::setGeometry(rect);
    layOut(m_items, rect.marginsRemoved(contentsMargins()), effectiveSpacing(*this), true);
}

Qt::Orientations FlowRow::expandingDirections() const { return {}; }

bool FlowRow::hasHeightForWidth() const { return true; }

int FlowRow::heightForWidth(int width) const
{
    const QMargins margins = contentsMargins();
    const QRect rect(0, 0, width - margins.left() - margins.right(), 0);
    return layOut(m_items, rect, effectiveSpacing(*this), false) + margins.top() + margins.bottom();
}

} // namespace mirante::widgets
