#pragma once

#include <QJsonObject>
#include <QString>
#include <Qt>

#include <functional>
#include <optional>
#include <vector>

namespace mirante {

// What a cell shows. See docs/data-model.md#layout-tree.
//
// - In a task dashboard: { item }.
// - In a root dashboard: { task, dashboard } or { item, task }.
//
// Empty strings mean "not set".
struct CellRef
{
    QString item;
    QString task;
    QString dashboard;

    bool isTask() const { return item.isEmpty() && !task.isEmpty(); }
    bool isItem() const { return !item.isEmpty(); }

    friend bool operator==(const CellRef &a, const CellRef &b)
    {
        return a.item == b.item && a.task == b.task && a.dashboard == b.dashboard;
    }
    friend bool operator!=(const CellRef &a, const CellRef &b) { return !(a == b); }
};

// One node of a layout tree: a split, a tab group or a cell.
// The tree is a value. Edits copy it, change the copy and return it.
struct LayoutNode
{
    enum class Kind { Split, Tabs, Cell };
    Kind kind = Kind::Cell;

    // Split
    Qt::Orientation dir = Qt::Horizontal;   // Horizontal is "h", side by side
    std::vector<double> sizes;              // ratios, one per child

    // Split and Tabs
    std::vector<LayoutNode> children;

    // Tabs
    std::vector<QString> titles;            // one per child
    int current = 0;
    bool collapseSingle = false;

    // Cell
    std::optional<CellRef> ref;             // nullopt is an empty cell
    bool removed = false;                   // set by normalisation, never written

    // Keys this version does not know, written back unchanged.
    QJsonObject extra;
};

// A node's position: the child indices from the root. The root is {}.
using NodePath = std::vector<int>;

enum class Edge { Left, Right, Top, Bottom };

// Where a dragged node or reference lands on a target node.
struct DropZone
{
    // Replace puts the dropped content in the target cell instead of what
    // the cell held. A drag with Ctrl held gives it.
    enum class Kind { Edge, Centre, TabBar, Replace };
    Kind kind = Kind::Centre;
    Edge edge = Edge::Right;    // for Kind::Edge
    int tabIndex = 0;           // for Kind::TabBar: the insert position
};

// The result of an edit: the new tree, and the path of the node the edit
// made or moved, in the new tree before normalisation.
struct EditResult
{
    LayoutNode tree;
    NodePath path;
};

// Says whether a reference still refers to something. Model::refExists() is one.
using RefExists = std::function<bool(const CellRef &)>;

// JSON conversion. Implemented in LayoutTreeJson.cpp.
CellRef cellRefFromJson(const QJsonObject &json);
QJsonObject cellRefToJson(const CellRef &ref);
LayoutNode layoutFromJson(const QJsonObject &json);
QJsonObject layoutToJson(const LayoutNode &node);

namespace layout {

// Tree access and edits. Implemented in LayoutTree.cpp.
// Paths given to an edit must be valid for the tree. An edit never normalises.

LayoutNode emptyLayout();                                   // one empty cell
const LayoutNode *nodeAt(const LayoutNode &tree, const NodePath &path);
std::vector<NodePath> cellPaths(const LayoutNode &tree);    // in depth-first order

// Splits the node at path, putting a new cell with ref on the given edge.
EditResult splitCell(const LayoutNode &tree, const NodePath &path, Edge edge,
                     const std::optional<CellRef> &ref);
// Wraps the node at path in a tab group with one tab.
EditResult wrapInTabs(const LayoutNode &tree, const NodePath &path, const QString &title);
// Adds a tab with ref to the tab group holding the node at path, or wraps the
// node in a tab group first if it is not in one. Returns the new tab's path.
EditResult addTab(const LayoutNode &tree, const NodePath &path, const std::optional<CellRef> &ref,
                  const QString &title);
EditResult insertTab(const LayoutNode &tree, const NodePath &tabsPath, int index,
                     const std::optional<CellRef> &ref, const QString &title);
LayoutNode setRef(const LayoutNode &tree, const NodePath &path, const std::optional<CellRef> &ref);
LayoutNode removeNode(const LayoutNode &tree, const NodePath &path);
// Moves the node at from to the zone of the node at to. Returns the moved node's path.
EditResult moveNode(const LayoutNode &tree, const NodePath &from, const NodePath &to,
                    const DropZone &zone);
LayoutNode renameTab(const LayoutNode &tree, const NodePath &tabsPath, int index, const QString &title);
LayoutNode moveTab(const LayoutNode &tree, const NodePath &tabsPath, int from, int to);
LayoutNode setCurrentTab(const LayoutNode &tree, const NodePath &tabsPath, int index);
LayoutNode setCollapseSingle(const LayoutNode &tree, const NodePath &tabsPath, bool collapse);
LayoutNode setSizes(const LayoutNode &tree, const NodePath &splitPath, const std::vector<double> &sizes);

// True if both trees have the same nodes and references, ignoring sizes and
// current tabs. LayoutView uses it to tell a resize from a structural change.
bool sameShape(const LayoutNode &a, const LayoutNode &b);

// Applies the normalisation rules of docs/data-model.md#normalisation until
// nothing changes. If track is given, it is set to the tracked node's new path
// in the result, or to nullopt if the node is gone. A path that does not lead to
// a node of the input sets nullopt too. Implemented in LayoutNormalise.cpp.
LayoutNode normalise(const LayoutNode &tree, const RefExists &exists,
                     std::optional<NodePath> *track = nullptr);

} // namespace layout

} // namespace mirante
