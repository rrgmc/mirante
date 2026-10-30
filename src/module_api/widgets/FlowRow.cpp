#include <mirante/widgets/FlowRow.h>

namespace mirante::widgets {

FlowRow::FlowRow(QWidget *parent)
    : QLayout(parent)
{
}

FlowRow::~FlowRow()
{
    while (QLayoutItem *item = takeAt(0))
        delete item;
}

void FlowRow::addItem(QLayoutItem *item) { m_items.append(item); }

int FlowRow::count() const { return int(m_items.size()); }

QLayoutItem *FlowRow::itemAt(int index) const { return m_items.value(index); }

QLayoutItem *FlowRow::takeAt(int index)
{
    return index >= 0 && index < m_items.size() ? m_items.takeAt(index) : nullptr;
}

QSize FlowRow::sizeHint() const { return minimumSize(); }

QSize FlowRow::minimumSize() const { return {}; }

void FlowRow::setGeometry(const QRect &rect) { QLayout::setGeometry(rect); }

Qt::Orientations FlowRow::expandingDirections() const { return {}; }

bool FlowRow::hasHeightForWidth() const { return true; }

int FlowRow::heightForWidth(int width) const
{
    Q_UNUSED(width)
    return 0;
}

} // namespace mirante::widgets
