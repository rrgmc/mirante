// Stub from task T0. Task T4 implements this file.
#include "core/LayoutTree.h"

namespace mirante::layout {

LayoutNode emptyLayout() { return {}; }
const LayoutNode *nodeAt(const LayoutNode & /*tree*/, const NodePath & /*path*/) { return nullptr; }
std::vector<NodePath> cellPaths(const LayoutNode & /*tree*/) { return {}; }

EditResult splitCell(const LayoutNode &tree, const NodePath & /*path*/, Edge /*edge*/,
                     const std::optional<CellRef> & /*ref*/)
{
    return {tree, {}};
}

EditResult wrapInTabs(const LayoutNode &tree, const NodePath & /*path*/, const QString & /*title*/)
{
    return {tree, {}};
}

EditResult addTab(const LayoutNode &tree, const NodePath & /*path*/, const std::optional<CellRef> & /*ref*/,
                  const QString & /*title*/)
{
    return {tree, {}};
}

EditResult insertTab(const LayoutNode &tree, const NodePath & /*tabsPath*/, int /*index*/,
                     const std::optional<CellRef> & /*ref*/, const QString & /*title*/)
{
    return {tree, {}};
}

LayoutNode setRef(const LayoutNode &tree, const NodePath & /*path*/, const std::optional<CellRef> & /*ref*/)
{
    return tree;
}

LayoutNode removeNode(const LayoutNode &tree, const NodePath & /*path*/) { return tree; }

EditResult moveNode(const LayoutNode &tree, const NodePath & /*from*/, const NodePath & /*to*/,
                    const DropZone & /*zone*/)
{
    return {tree, {}};
}

LayoutNode renameTab(const LayoutNode &tree, const NodePath & /*tabsPath*/, int /*index*/,
                     const QString & /*title*/)
{
    return tree;
}

LayoutNode moveTab(const LayoutNode &tree, const NodePath & /*tabsPath*/, int /*from*/, int /*to*/)
{
    return tree;
}

LayoutNode setCurrentTab(const LayoutNode &tree, const NodePath & /*tabsPath*/, int /*index*/)
{
    return tree;
}

LayoutNode setCollapseSingle(const LayoutNode &tree, const NodePath & /*tabsPath*/, bool /*collapse*/)
{
    return tree;
}

LayoutNode setSizes(const LayoutNode &tree, const NodePath & /*splitPath*/, const std::vector<double> & /*sizes*/)
{
    return tree;
}

bool sameShape(const LayoutNode & /*a*/, const LayoutNode & /*b*/) { return false; }

} // namespace mirante::layout
