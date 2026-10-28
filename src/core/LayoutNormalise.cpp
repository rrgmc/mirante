// The normalisation rules of docs/data-model.md#normalisation.
//
// Each pass walks the tree bottom-up and applies every rule once. normalise()
// repeats passes until one changes nothing.
//
// Besides the six rules, a pass makes the tree valid:
// - a split with no children becomes an empty cell;
// - split sizes with the wrong count, or with a value that is not positive and
//   finite, become equal sizes;
// - a tab group's titles are padded with empty titles, or cut, to one per tab;
// - a tab group's current tab is clamped into range.
//
// Rule 5 needs no work here: the editor clears a cell with setRef(), and
// normalisation keeps empty cells as cells.
//
// Tracking. The tracked node keeps its identity while the tree changes around
// it, and its path is updated as siblings are merged or wrappers removed.
// - A split or tab group replaced by its only child (rules 1 and 4), or a split
//   absorbed by its parent (rule 2), is gone: tracking it clears the path.
//   Tracking that child, or a node inside it, follows it to its new path.
// - A split or tab group with no children turns into an empty cell in the same
//   place (rule 3). That counts as the same node, so its path is kept.
// - A cell whose reference is gone stays in place (rule 6), so its path is kept.
// - A path that does not lead to a node of the input tree is cleared.
// A cleared path is empty. The extra keys of a removed wrapper are dropped.

#include "core/LayoutTree.h"

#include <cmath>
#include <cstddef>
#include <optional>
#include <utility>

namespace mirante::layout {

namespace {

// A tracked path relative to the node being normalised. nullopt once the
// tracked node is gone.
using Track = std::optional<NodePath>;

int toInt(std::size_t i)
{
    return static_cast<int>(i);
}

NodePath tail(const NodePath &path)
{
    return NodePath(path.begin() + 1, path.end());
}

NodePath prepend(int index, const NodePath &rest)
{
    NodePath path;
    path.reserve(rest.size() + 1);
    path.push_back(index);
    path.insert(path.end(), rest.begin(), rest.end());
    return path;
}

bool validPath(const LayoutNode &tree, const NodePath &path)
{
    const LayoutNode *node = &tree;
    for (int index : path) {
        if (node->kind == LayoutNode::Kind::Cell || index < 0
            || static_cast<std::size_t>(index) >= node->children.size())
            return false;
        node = &node->children[static_cast<std::size_t>(index)];
    }
    return true;
}

LayoutNode emptyCell()
{
    return LayoutNode();
}

bool validSizes(const LayoutNode &split)
{
    if (split.sizes.size() != split.children.size())
        return false;
    for (double size : split.sizes) {
        if (!std::isfinite(size) || size <= 0.0)
            return false;
    }
    return true;
}

double sum(const std::vector<double> &values)
{
    double total = 0.0;
    for (double value : values)
        total += value;
    return total;
}

// The tracked node inside a split or tab group, while its children change.
// child is the index of the child that holds it, rest the path inside that child.
struct ChildTrack
{
    int child = -1;
    NodePath rest;
};

class Normaliser
{
public:
    explicit Normaliser(const RefExists &exists) : m_exists(exists) {}

    // One pass over node. track is null if the tracked node is not node or
    // inside it. Returns true if anything changed.
    bool pass(LayoutNode &node, Track *track)
    {
        if (node.kind == LayoutNode::Kind::Split)
            return split(node, track);
        if (node.kind == LayoutNode::Kind::Tabs)
            return tabs(node, track);
        return cell(node);
    }

private:
    // Rule 6.
    bool cell(LayoutNode &node)
    {
        if (!node.ref || !m_exists || m_exists(*node.ref))
            return false;
        node.ref.reset();
        node.removed = true;
        return true;
    }

    // Normalises the children of node. Returns true if any changed, and fills
    // inner with the tracked node's position, or clears track if it is gone.
    bool children(LayoutNode &node, Track *track, std::optional<ChildTrack> &inner)
    {
        bool changed = false;
        for (std::size_t i = 0; i < node.children.size(); ++i) {
            if (track && *track && !(*track)->empty() && (**track)[0] == toInt(i)) {
                Track childTrack = tail(**track);
                changed |= pass(node.children[i], &childTrack);
                if (childTrack)
                    inner = ChildTrack{toInt(i), *childTrack};
                else
                    track->reset();
            } else {
                changed |= pass(node.children[i], nullptr);
            }
        }
        return changed;
    }

    // Replaces node by its only child, for rules 1 and 4.
    static void replaceByChild(LayoutNode &node, Track *track, const std::optional<ChildTrack> &inner)
    {
        LayoutNode child = std::move(node.children.front());
        node = std::move(child);
        if (track && *track)
            *track = inner ? Track(inner->rest) : std::nullopt;
    }

    static void becomeEmptyCell(LayoutNode &node)
    {
        node = emptyCell();
    }

    bool split(LayoutNode &node, Track *track)
    {
        if (node.children.empty()) {
            becomeEmptyCell(node);
            return true;
        }

        bool changed = false;
        if (!validSizes(node)) {
            node.sizes.assign(node.children.size(), 1.0);
            changed = true;
        }

        std::optional<ChildTrack> inner;
        changed |= children(node, track, inner);

        // Rule 2: absorb child splits of the same direction.
        std::vector<LayoutNode> merged;
        std::vector<double> sizes;
        std::optional<ChildTrack> mergedInner;
        for (std::size_t i = 0; i < node.children.size(); ++i) {
            LayoutNode &child = node.children[i];
            const bool tracked = inner && inner->child == toInt(i);
            if (child.kind == LayoutNode::Kind::Split && child.dir == node.dir) {
                changed = true;
                if (tracked) {
                    if (inner->rest.empty())
                        track->reset();
                    else
                        mergedInner = ChildTrack{toInt(merged.size()) + inner->rest[0], tail(inner->rest)};
                }
                const double total = sum(child.sizes);
                for (std::size_t j = 0; j < child.children.size(); ++j) {
                    merged.push_back(std::move(child.children[j]));
                    sizes.push_back(node.sizes[i] * child.sizes[j] / total);
                }
            } else {
                if (tracked)
                    mergedInner = ChildTrack{toInt(merged.size()), inner->rest};
                merged.push_back(std::move(child));
                sizes.push_back(node.sizes[i]);
            }
        }
        node.children = std::move(merged);
        node.sizes = std::move(sizes);
        inner = mergedInner;

        // Rule 1.
        if (node.children.size() == 1) {
            replaceByChild(node, track, inner);
            return true;
        }

        if (track && *track && inner)
            *track = prepend(inner->child, inner->rest);
        return changed;
    }

    bool tabs(LayoutNode &node, Track *track)
    {
        // Rule 3.
        if (node.children.empty()) {
            becomeEmptyCell(node);
            return true;
        }

        bool changed = false;
        if (node.titles.size() != node.children.size()) {
            node.titles.resize(node.children.size());
            changed = true;
        }
        const int last = toInt(node.children.size()) - 1;
        if (node.current < 0 || node.current > last) {
            node.current = node.current < 0 ? 0 : last;
            changed = true;
        }

        std::optional<ChildTrack> inner;
        changed |= children(node, track, inner);

        // Rule 4.
        if (node.children.size() == 1 && node.collapseSingle) {
            replaceByChild(node, track, inner);
            return true;
        }

        if (track && *track && inner)
            *track = prepend(inner->child, inner->rest);
        return changed;
    }

    const RefExists &m_exists;
};

} // namespace

LayoutNode normalise(const LayoutNode &tree, const RefExists &exists, NodePath *track)
{
    LayoutNode result = tree;
    Track tracked;
    if (track && validPath(tree, *track))
        tracked = *track;

    Normaliser normaliser(exists);
    bool changed = true;
    while (changed)
        changed = normaliser.pass(result, tracked ? &tracked : nullptr);

    if (track)
        *track = tracked ? *tracked : NodePath();
    return result;
}

} // namespace mirante::layout
