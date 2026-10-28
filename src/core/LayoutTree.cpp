#include "core/LayoutTree.h"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace mirante::layout {

namespace {

using Kind = LayoutNode::Kind;

std::size_t idx(int i) { return static_cast<std::size_t>(i); }
int count(const std::vector<LayoutNode> &children) { return static_cast<int>(children.size()); }

bool inRange(int i, int size) { return i >= 0 && i < size; }

LayoutNode *mutableAt(LayoutNode &tree, const NodePath &path)
{
    LayoutNode *node = &tree;
    for (int i : path) {
        if (node->kind == Kind::Cell || !inRange(i, count(node->children)))
            return nullptr;
        node = &node->children[idx(i)];
    }
    return node;
}

// Keeps titles as long as children, so tab edits can index both.
void fitTitles(LayoutNode &tabs) { tabs.titles.resize(tabs.children.size()); }

LayoutNode cell(const std::optional<CellRef> &ref)
{
    LayoutNode node;
    node.kind = Kind::Cell;
    node.ref = ref;
    return node;
}

LayoutNode tabsOf(LayoutNode child, const QString &title)
{
    LayoutNode tabs;
    tabs.kind = Kind::Tabs;
    tabs.children.push_back(std::move(child));
    tabs.titles.push_back(title);
    return tabs;
}

NodePath parentOf(const NodePath &path) { return NodePath(path.begin(), path.end() - 1); }

NodePath childOf(NodePath path, int index)
{
    path.push_back(index);
    return path;
}

bool isPrefix(const NodePath &prefix, const NodePath &path)
{
    return prefix.size() <= path.size() && std::equal(prefix.begin(), prefix.end(), path.begin());
}

// Removes the child at index from a split or tab group.
void removeChild(LayoutNode &parent, int index)
{
    if (parent.kind == Kind::Tabs)
        fitTitles(parent);
    parent.children.erase(parent.children.begin() + index);
    if (parent.kind == Kind::Split) {
        if (idx(index) < parent.sizes.size())
            parent.sizes.erase(parent.sizes.begin() + index);
    } else {
        parent.titles.erase(parent.titles.begin() + index);
        if (index < parent.current)
            --parent.current;
        parent.current = std::clamp(parent.current, 0, std::max(0, count(parent.children) - 1));
    }
}

// Inserts a tab at index, which must be in [0, size], and makes it current.
void insertTabAt(LayoutNode &tabs, int index, LayoutNode node, const QString &title)
{
    fitTitles(tabs);
    tabs.children.insert(tabs.children.begin() + index, std::move(node));
    tabs.titles.insert(tabs.titles.begin() + index, title);
    tabs.current = index;
}

// Replaces the node at path with a split holding it and node, node on edge.
// This always nests a new split, even when the parent split has the same
// direction; normalisation rule 2 then merges it into the parent, giving each
// part half of the old node's share.
NodePath splitAt(LayoutNode &tree, const NodePath &path, Edge edge, LayoutNode node)
{
    LayoutNode *target = mutableAt(tree, path);
    LayoutNode split;
    split.kind = Kind::Split;
    split.dir = (edge == Edge::Left || edge == Edge::Right) ? Qt::Horizontal : Qt::Vertical;
    split.sizes = {1.0, 1.0};
    const bool first = edge == Edge::Left || edge == Edge::Top;
    if (first) {
        split.children.push_back(std::move(node));
        split.children.push_back(std::move(*target));
    } else {
        split.children.push_back(std::move(*target));
        split.children.push_back(std::move(node));
    }
    *target = std::move(split);
    return childOf(path, first ? 0 : 1);
}

// Adds node as a tab after the node at path if its parent is a tab group, or
// wraps the node at path in a tab group with an empty title first.
NodePath addTabAt(LayoutNode &tree, const NodePath &path, LayoutNode node, const QString &title)
{
    if (!path.empty()) {
        const NodePath parentPath = parentOf(path);
        LayoutNode *parent = mutableAt(tree, parentPath);
        if (parent->kind == Kind::Tabs) {
            const int index = path.back() + 1;
            insertTabAt(*parent, index, std::move(node), title);
            return childOf(parentPath, index);
        }
    }
    LayoutNode *target = mutableAt(tree, path);
    *target = tabsOf(std::move(*target), QString());
    insertTabAt(*target, 1, std::move(node), title);
    return childOf(path, 1);
}

} // namespace

LayoutNode emptyLayout() { return cell(std::nullopt); }

const LayoutNode *nodeAt(const LayoutNode &tree, const NodePath &path)
{
    return mutableAt(const_cast<LayoutNode &>(tree), path);
}

std::vector<NodePath> cellPaths(const LayoutNode &tree)
{
    std::vector<NodePath> paths;
    NodePath path;
    std::function<void(const LayoutNode &)> visit = [&](const LayoutNode &node) {
        if (node.kind == Kind::Cell) {
            paths.push_back(path);
            return;
        }
        for (int i = 0; i < count(node.children); ++i) {
            path.push_back(i);
            visit(node.children[idx(i)]);
            path.pop_back();
        }
    };
    visit(tree);
    return paths;
}

EditResult splitCell(const LayoutNode &tree, const NodePath &path, Edge edge,
                     const std::optional<CellRef> &ref)
{
    if (!nodeAt(tree, path))
        return {tree, {}};
    LayoutNode result = tree;
    const NodePath newPath = splitAt(result, path, edge, cell(ref));
    return {std::move(result), newPath};
}

// Returns the path of the new tab group, which is the old node's path.
EditResult wrapInTabs(const LayoutNode &tree, const NodePath &path, const QString &title)
{
    if (!nodeAt(tree, path))
        return {tree, {}};
    LayoutNode result = tree;
    LayoutNode *target = mutableAt(result, path);
    *target = tabsOf(std::move(*target), title);
    return {std::move(result), path};
}

EditResult addTab(const LayoutNode &tree, const NodePath &path, const std::optional<CellRef> &ref,
                  const QString &title)
{
    if (!nodeAt(tree, path))
        return {tree, {}};
    LayoutNode result = tree;
    const NodePath newPath = addTabAt(result, path, cell(ref), title);
    return {std::move(result), newPath};
}

EditResult insertTab(const LayoutNode &tree, const NodePath &tabsPath, int index,
                     const std::optional<CellRef> &ref, const QString &title)
{
    const LayoutNode *tabs = nodeAt(tree, tabsPath);
    if (!tabs || tabs->kind != Kind::Tabs || index < 0 || index > count(tabs->children))
        return {tree, {}};
    LayoutNode result = tree;
    insertTabAt(*mutableAt(result, tabsPath), index, cell(ref), title);
    return {std::move(result), childOf(tabsPath, index)};
}

LayoutNode setRef(const LayoutNode &tree, const NodePath &path, const std::optional<CellRef> &ref)
{
    const LayoutNode *node = nodeAt(tree, path);
    if (!node || node->kind != Kind::Cell)
        return tree;
    LayoutNode result = tree;
    LayoutNode *target = mutableAt(result, path);
    target->ref = ref;
    target->removed = false;
    return result;
}

// Removing the root leaves one empty cell.
LayoutNode removeNode(const LayoutNode &tree, const NodePath &path)
{
    if (!nodeAt(tree, path))
        return tree;
    if (path.empty())
        return emptyLayout();
    LayoutNode result = tree;
    removeChild(*mutableAt(result, parentOf(path)), path.back());
    return result;
}

// Zones:
// - Edge splits the target, putting the moved node on that edge.
// - Centre on an empty or removed cell replaces the cell with the moved node.
//   Elsewhere it adds the moved node as a tab, like addTab.
// - TabBar inserts the moved node into the target tab group at tabIndex, an
//   index in the tree before the move.
// - Replace puts the moved node in place of the target cell, whose content
//   is dropped. The target must be a cell.
// A node moved out of a tab group keeps its tab title.
EditResult moveNode(const LayoutNode &tree, const NodePath &from, const NodePath &to,
                    const DropZone &zone)
{
    const LayoutNode *source = nodeAt(tree, from);
    const LayoutNode *target = nodeAt(tree, to);
    if (!source || !target || isPrefix(from, to))
        return {tree, {}};
    if (zone.kind == DropZone::Kind::TabBar
        && (target->kind != Kind::Tabs || zone.tabIndex < 0 || zone.tabIndex > count(target->children)))
        return {tree, {}};
    if (zone.kind == DropZone::Kind::Replace && target->kind != Kind::Cell)
        return {tree, {}};

    const NodePath fromParent = parentOf(from);
    const int fromIndex = from.back();
    QString title;
    const LayoutNode *sourceParent = nodeAt(tree, fromParent);
    if (sourceParent->kind == Kind::Tabs && idx(fromIndex) < sourceParent->titles.size())
        title = sourceParent->titles[idx(fromIndex)];

    // Removing the source shifts later siblings, and so the target path, back by one.
    NodePath target2 = to;
    if (isPrefix(fromParent, to) && to.size() > fromParent.size() && to[fromParent.size()] > fromIndex)
        --target2[fromParent.size()];
    int tabIndex = zone.tabIndex;
    if (zone.kind == DropZone::Kind::TabBar && fromParent == to && fromIndex < tabIndex)
        --tabIndex;

    LayoutNode result = tree;
    LayoutNode moved = std::move(*mutableAt(result, from));
    removeChild(*mutableAt(result, fromParent), fromIndex);

    NodePath newPath;
    switch (zone.kind) {
    case DropZone::Kind::Edge:
        newPath = splitAt(result, target2, zone.edge, std::move(moved));
        break;
    case DropZone::Kind::Centre: {
        LayoutNode *dest = mutableAt(result, target2);
        if (dest->kind == Kind::Cell && (!dest->ref || dest->removed)) {
            *dest = std::move(moved);
            newPath = target2;
        } else {
            newPath = addTabAt(result, target2, std::move(moved), title);
        }
        break;
    }
    case DropZone::Kind::TabBar:
        insertTabAt(*mutableAt(result, target2), tabIndex, std::move(moved), title);
        newPath = childOf(target2, tabIndex);
        break;
    case DropZone::Kind::Replace:
        *mutableAt(result, target2) = std::move(moved);
        newPath = target2;
        break;
    }
    return {std::move(result), newPath};
}

LayoutNode renameTab(const LayoutNode &tree, const NodePath &tabsPath, int index, const QString &title)
{
    const LayoutNode *tabs = nodeAt(tree, tabsPath);
    if (!tabs || tabs->kind != Kind::Tabs || !inRange(index, count(tabs->children)))
        return tree;
    LayoutNode result = tree;
    LayoutNode *target = mutableAt(result, tabsPath);
    fitTitles(*target);
    target->titles[idx(index)] = title;
    return result;
}

// The current tab stays current, wherever it moves to.
LayoutNode moveTab(const LayoutNode &tree, const NodePath &tabsPath, int from, int to)
{
    const LayoutNode *tabs = nodeAt(tree, tabsPath);
    if (!tabs || tabs->kind != Kind::Tabs || !inRange(from, count(tabs->children))
        || !inRange(to, count(tabs->children)))
        return tree;
    LayoutNode result = tree;
    LayoutNode *target = mutableAt(result, tabsPath);
    fitTitles(*target);
    LayoutNode node = std::move(target->children[idx(from)]);
    QString title = target->titles[idx(from)];
    target->children.erase(target->children.begin() + from);
    target->titles.erase(target->titles.begin() + from);
    target->children.insert(target->children.begin() + to, std::move(node));
    target->titles.insert(target->titles.begin() + to, title);

    int &current = target->current;
    if (current == from)
        current = to;
    else if (from < current && current <= to)
        --current;
    else if (to <= current && current < from)
        ++current;
    return result;
}

LayoutNode setCurrentTab(const LayoutNode &tree, const NodePath &tabsPath, int index)
{
    const LayoutNode *tabs = nodeAt(tree, tabsPath);
    if (!tabs || tabs->kind != Kind::Tabs || !inRange(index, count(tabs->children)))
        return tree;
    LayoutNode result = tree;
    mutableAt(result, tabsPath)->current = index;
    return result;
}

LayoutNode setCollapseSingle(const LayoutNode &tree, const NodePath &tabsPath, bool collapse)
{
    const LayoutNode *tabs = nodeAt(tree, tabsPath);
    if (!tabs || tabs->kind != Kind::Tabs)
        return tree;
    LayoutNode result = tree;
    mutableAt(result, tabsPath)->collapseSingle = collapse;
    return result;
}

LayoutNode setSizes(const LayoutNode &tree, const NodePath &splitPath, const std::vector<double> &sizes)
{
    const LayoutNode *split = nodeAt(tree, splitPath);
    if (!split || split->kind != Kind::Split || sizes.size() != split->children.size())
        return tree;
    LayoutNode result = tree;
    mutableAt(result, splitPath)->sizes = sizes;
    return result;
}

bool sameShape(const LayoutNode &a, const LayoutNode &b)
{
    if (a.kind != b.kind || a.children.size() != b.children.size())
        return false;
    switch (a.kind) {
    case Kind::Cell:
        return a.ref == b.ref && a.removed == b.removed;
    case Kind::Split:
        if (a.dir != b.dir)
            return false;
        break;
    case Kind::Tabs:
        if (a.titles != b.titles || a.collapseSingle != b.collapseSingle)
            return false;
        break;
    }
    for (std::size_t i = 0; i < a.children.size(); ++i) {
        if (!sameShape(a.children[i], b.children[i]))
            return false;
    }
    return true;
}

} // namespace mirante::layout
