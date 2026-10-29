#include "layout/Presets.h"

#include <QCoreApplication>

#include <algorithm>
#include <utility>

namespace mirante::presets {

namespace {

using Kind = LayoutNode::Kind;

enum Preset { Full, SideBySide, Stacked, Grid, LargeTop, LargeLeft, Tabs, PresetCount };

// Hands out the items in order, then empty references.
class Filler
{
public:
    explicit Filler(const QStringList &itemIds)
        : m_itemIds(itemIds)
    {
    }

    LayoutNode cell()
    {
        LayoutNode node;
        node.kind = Kind::Cell;
        if (m_next < m_itemIds.size())
            node.ref = CellRef{m_itemIds.at(m_next++), {}, {}};
        return node;
    }

private:
    const QStringList &m_itemIds;
    qsizetype m_next = 0;
};

LayoutNode split(Qt::Orientation dir, std::vector<double> sizes, std::vector<LayoutNode> children)
{
    LayoutNode node;
    node.kind = Kind::Split;
    node.dir = dir;
    node.sizes = std::move(sizes);
    node.children = std::move(children);
    return node;
}

LayoutNode tabs(const QStringList &itemIds, const QStringList &itemNames)
{
    LayoutNode node;
    node.kind = Kind::Tabs;
    Filler filler(itemIds);
    const qsizetype count = std::max<qsizetype>(itemIds.size(), 1);
    for (qsizetype i = 0; i < count; ++i) {
        node.children.push_back(filler.cell());
        node.titles.push_back(i < itemNames.size() ? itemNames.at(i) : QString());
    }
    return node;
}

} // namespace

QStringList names()
{
    return {
        QCoreApplication::translate("Presets", "Full"),
        QCoreApplication::translate("Presets", "Two side by side"),
        QCoreApplication::translate("Presets", "Two stacked"),
        QCoreApplication::translate("Presets", "Grid of four"),
        QCoreApplication::translate("Presets", "One large on top, two below"),
        QCoreApplication::translate("Presets", "One large on the left, two stacked on the right"),
        QCoreApplication::translate("Presets", "Tabs"),
    };
}

LayoutNode build(int index, const QStringList &itemIds, const QStringList &itemNames)
{
    Filler f(itemIds);
    // Each child is made in its own statement, so the cells are filled in
    // depth-first order whatever order the compiler evaluates arguments in.
    switch (index) {
    case Full:
        return f.cell();
    case SideBySide:
    case Stacked: {
        LayoutNode first = f.cell();
        LayoutNode second = f.cell();
        return split(index == SideBySide ? Qt::Horizontal : Qt::Vertical, {1.0, 1.0},
                     {std::move(first), std::move(second)});
    }
    case Grid: {
        std::vector<LayoutNode> rows;
        for (int row = 0; row < 2; ++row) {
            LayoutNode left = f.cell();
            LayoutNode right = f.cell();
            rows.push_back(split(Qt::Horizontal, {1.0, 1.0}, {std::move(left), std::move(right)}));
        }
        return split(Qt::Vertical, {1.0, 1.0}, std::move(rows));
    }
    case LargeTop:
    case LargeLeft: {
        // The large cell takes two thirds.
        const bool top = index == LargeTop;
        LayoutNode large = f.cell();
        LayoutNode first = f.cell();
        LayoutNode second = f.cell();
        LayoutNode rest = split(top ? Qt::Horizontal : Qt::Vertical, {1.0, 1.0},
                                {std::move(first), std::move(second)});
        return split(top ? Qt::Vertical : Qt::Horizontal, {2.0, 1.0}, {std::move(large), std::move(rest)});
    }
    case Tabs:
        return tabs(itemIds, itemNames);
    default:
        break;
    }
    static_assert(PresetCount == 7, "names() lists every preset");
    return layout::emptyLayout();
}

} // namespace mirante::presets
