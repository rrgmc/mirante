// Tests the layout tree edit operations and sameShape, on raw (not normalised) output.
#include "core/LayoutTree.h"

#include <QTest>

using namespace mirante;
using Kind = LayoutNode::Kind;

namespace {

CellRef item(const QString &id) { return CellRef{id, QString(), QString()}; }

LayoutNode cell(const QString &id)
{
    LayoutNode node;
    node.ref = item(id);
    return node;
}

LayoutNode split(Qt::Orientation dir, std::vector<LayoutNode> children)
{
    LayoutNode node;
    node.kind = Kind::Split;
    node.dir = dir;
    node.sizes.assign(children.size(), 1.0);
    node.children = std::move(children);
    return node;
}

LayoutNode tabs(std::vector<LayoutNode> children, std::vector<QString> titles, int current = 0)
{
    LayoutNode node;
    node.kind = Kind::Tabs;
    node.children = std::move(children);
    node.titles = std::move(titles);
    node.current = current;
    return node;
}

// The item id of the cell at path, or "?" if there is none.
QString itemAt(const LayoutNode &tree, const NodePath &path)
{
    const LayoutNode *node = layout::nodeAt(tree, path);
    if (!node || node->kind != Kind::Cell)
        return QStringLiteral("?");
    return node->ref ? node->ref->item : QString();
}

// h[ a, v[ b, c ], tabs{ "D": d, "E": e } ]
LayoutNode sample()
{
    return split(Qt::Horizontal,
                 {cell("a"), split(Qt::Vertical, {cell("b"), cell("c")}),
                  tabs({cell("d"), cell("e")}, {"D", "E"}, 1)});
}

DropZone edgeZone(Edge edge)
{
    DropZone zone;
    zone.kind = DropZone::Kind::Edge;
    zone.edge = edge;
    return zone;
}

DropZone centreZone() { return DropZone{}; }

DropZone tabBarZone(int index)
{
    DropZone zone;
    zone.kind = DropZone::Kind::TabBar;
    zone.tabIndex = index;
    return zone;
}

} // namespace

class TestLayoutEdits : public QObject
{
    Q_OBJECT

private slots:
    void emptyLayoutIsOneEmptyCell()
    {
        const LayoutNode tree = layout::emptyLayout();
        QCOMPARE(tree.kind, Kind::Cell);
        QVERIFY(!tree.ref);
        QVERIFY(!tree.removed);
    }

    void nodeAt()
    {
        const LayoutNode tree = sample();
        QVERIFY(layout::nodeAt(tree, {}) == &tree);
        QCOMPARE(itemAt(tree, {1, 1}), QStringLiteral("c"));
        QCOMPARE(itemAt(tree, {2, 0}), QStringLiteral("d"));
        QVERIFY(!layout::nodeAt(tree, {3}));
        QVERIFY(!layout::nodeAt(tree, {-1}));
        QVERIFY(!layout::nodeAt(tree, {0, 0}));   // a cell has no children
    }

    void cellPathsDepthFirst()
    {
        const std::vector<NodePath> expected = {{0}, {1, 0}, {1, 1}, {2, 0}, {2, 1}};
        QVERIFY(layout::cellPaths(sample()) == expected);
        QVERIFY(layout::cellPaths(layout::emptyLayout()) == std::vector<NodePath>{NodePath{}});
    }

    void splitCellEdges()
    {
        const LayoutNode tree = sample();
        struct Case { Edge edge; Qt::Orientation dir; int newIndex; };
        const Case cases[] = {{Edge::Left, Qt::Horizontal, 0}, {Edge::Right, Qt::Horizontal, 1},
                              {Edge::Top, Qt::Vertical, 0}, {Edge::Bottom, Qt::Vertical, 1}};
        for (const Case &c : cases) {
            const EditResult r = layout::splitCell(tree, {0}, c.edge, item("n"));
            const LayoutNode *s = layout::nodeAt(r.tree, {0});
            QCOMPARE(s->kind, Kind::Split);
            QCOMPARE(s->dir, c.dir);
            QVERIFY(s->sizes == std::vector<double>({1.0, 1.0}));
            QVERIFY(r.path == NodePath({0, c.newIndex}));
            QCOMPARE(itemAt(r.tree, r.path), QStringLiteral("n"));
            QCOMPARE(itemAt(r.tree, {0, 1 - c.newIndex}), QStringLiteral("a"));
        }
        QVERIFY(layout::sameShape(tree, sample()));   // input unchanged
    }

    // Splitting in the parent's direction nests a new split; normalisation merges it.
    void splitCellNestsInSameDirection()
    {
        const EditResult r = layout::splitCell(sample(), {1, 0}, Edge::Bottom, std::nullopt);
        QCOMPARE(layout::nodeAt(r.tree, {1})->children.size(), std::size_t(2));
        const LayoutNode *nested = layout::nodeAt(r.tree, {1, 0});
        QCOMPARE(nested->kind, Kind::Split);
        QCOMPARE(nested->dir, Qt::Vertical);
        QVERIFY(r.path == NodePath({1, 0, 1}));
        QCOMPARE(itemAt(r.tree, r.path), QString());
    }

    void splitRoot()
    {
        const EditResult r = layout::splitCell(layout::emptyLayout(), {}, Edge::Right, item("n"));
        QCOMPARE(r.tree.kind, Kind::Split);
        QVERIFY(r.path == NodePath({1}));
        QCOMPARE(itemAt(r.tree, {0}), QString());
    }

    void wrapInTabs()
    {
        const EditResult r = layout::wrapInTabs(sample(), {0}, "A");
        QVERIFY(r.path == NodePath({0}));
        const LayoutNode *t = layout::nodeAt(r.tree, {0});
        QCOMPARE(t->kind, Kind::Tabs);
        QVERIFY(t->titles == std::vector<QString>({"A"}));
        QCOMPARE(t->current, 0);
        QCOMPARE(itemAt(r.tree, {0, 0}), QStringLiteral("a"));
    }

    void addTabInsideTabGroup()
    {
        const EditResult r = layout::addTab(sample(), {2, 0}, item("n"), "N");
        QVERIFY(r.path == NodePath({2, 1}));
        const LayoutNode *t = layout::nodeAt(r.tree, {2});
        QVERIFY(t->titles == std::vector<QString>({"D", "N", "E"}));
        QCOMPARE(t->current, 1);
        QCOMPARE(itemAt(r.tree, {2, 2}), QStringLiteral("e"));
    }

    void addTabWrapsFirst()
    {
        const EditResult r = layout::addTab(sample(), {1, 1}, item("n"), "N");
        QVERIFY(r.path == NodePath({1, 1, 1}));
        const LayoutNode *t = layout::nodeAt(r.tree, {1, 1});
        QCOMPARE(t->kind, Kind::Tabs);
        QVERIFY(t->titles == std::vector<QString>({QString(), "N"}));
        QCOMPARE(t->current, 1);
        QCOMPARE(itemAt(r.tree, {1, 1, 0}), QStringLiteral("c"));
        QCOMPARE(itemAt(r.tree, r.path), QStringLiteral("n"));
    }

    void insertTab()
    {
        const EditResult r = layout::insertTab(sample(), {2}, 0, item("n"), "N");
        QVERIFY(r.path == NodePath({2, 0}));
        const LayoutNode *t = layout::nodeAt(r.tree, {2});
        QVERIFY(t->titles == std::vector<QString>({"N", "D", "E"}));
        QCOMPARE(t->current, 0);

        const EditResult end = layout::insertTab(sample(), {2}, 2, std::nullopt, "N");
        QVERIFY(end.path == NodePath({2, 2}));
        QCOMPARE(itemAt(end.tree, end.path), QString());

        QVERIFY(layout::insertTab(sample(), {2}, 3, std::nullopt, "N").path.empty());
        QVERIFY(layout::insertTab(sample(), {0}, 0, std::nullopt, "N").path.empty());   // not tabs
    }

    void setRef()
    {
        LayoutNode tree = sample();
        tree.children[0].removed = true;
        const LayoutNode r = layout::setRef(tree, {0}, item("x"));
        QCOMPARE(itemAt(r, {0}), QStringLiteral("x"));
        QVERIFY(!r.children[0].removed);
        QCOMPARE(itemAt(layout::setRef(tree, {0}, std::nullopt), {0}), QString());
        QVERIFY(layout::sameShape(layout::setRef(tree, {1}, item("x")), tree));   // not a cell
    }

    void removeFromSplit()
    {
        LayoutNode tree = sample();
        tree.sizes = {1, 2, 3};
        const LayoutNode r = layout::removeNode(tree, {1});
        QCOMPARE(r.children.size(), std::size_t(2));
        QVERIFY(r.sizes == std::vector<double>({1, 3}));
        QCOMPARE(layout::nodeAt(r, {1})->kind, Kind::Tabs);
    }

    void removeFromTabs()
    {
        // Removing the current last tab moves current back.
        LayoutNode r = layout::removeNode(sample(), {2, 1});
        QVERIFY(r.children[2].titles == std::vector<QString>({"D"}));
        QCOMPARE(r.children[2].current, 0);

        // Removing a tab before the current one keeps the same tab current.
        LayoutNode tree = sample();
        tree.children[2] = tabs({cell("d"), cell("e"), cell("f")}, {"D", "E", "F"}, 2);
        r = layout::removeNode(tree, {2, 0});
        QCOMPARE(r.children[2].current, 1);
        QCOMPARE(itemAt(r, {2, 1}), QStringLiteral("f"));
        QVERIFY(r.children[2].titles == std::vector<QString>({"E", "F"}));

        // Removing the only tab leaves an empty tab group for normalisation.
        r = layout::removeNode(r, {2, 0});
        r = layout::removeNode(r, {2, 0});
        QVERIFY(r.children[2].children.empty());
        QCOMPARE(r.children[2].current, 0);
    }

    void removeRoot()
    {
        const LayoutNode r = layout::removeNode(sample(), {});
        QCOMPARE(r.kind, Kind::Cell);
        QVERIFY(!r.ref);
    }

    void moveToEdge()
    {
        // a moves to the bottom of e. Removing a shifts the tabs from {2} to {1}.
        const EditResult r = layout::moveNode(sample(), {0}, {2, 1}, edgeZone(Edge::Bottom));
        QVERIFY(r.path == NodePath({1, 1, 1}));
        QCOMPARE(itemAt(r.tree, r.path), QStringLiteral("a"));
        QCOMPARE(itemAt(r.tree, {1, 1, 0}), QStringLiteral("e"));
        QCOMPARE(r.tree.children.size(), std::size_t(2));
        QCOMPARE(r.tree.sizes.size(), std::size_t(2));
    }

    void moveToEarlierSibling()
    {
        const EditResult r = layout::moveNode(sample(), {1, 1}, {0}, edgeZone(Edge::Left));
        QVERIFY(r.path == NodePath({0, 0}));
        QCOMPARE(itemAt(r.tree, r.path), QStringLiteral("c"));
        QCOMPARE(layout::nodeAt(r.tree, {1})->children.size(), std::size_t(1));
    }

    void moveToOwnParentEdge()
    {
        const EditResult r = layout::moveNode(sample(), {1, 0}, {1}, edgeZone(Edge::Right));
        QVERIFY(r.path == NodePath({1, 1}));
        QCOMPARE(itemAt(r.tree, {1, 0, 0}), QStringLiteral("c"));
        QCOMPARE(itemAt(r.tree, r.path), QStringLiteral("b"));
    }

    void moveCentreFillsEmptyCell()
    {
        LayoutNode tree = sample();
        tree.children[0].ref.reset();
        const EditResult r = layout::moveNode(tree, {1, 1}, {0}, centreZone());
        QVERIFY(r.path == NodePath({0}));
        QCOMPARE(itemAt(r.tree, {0}), QStringLiteral("c"));
        QCOMPARE(layout::nodeAt(r.tree, {1})->children.size(), std::size_t(1));
    }

    void moveCentreAddsTab()
    {
        // Into a cell that is not in a tab group: wraps it first.
        EditResult r = layout::moveNode(sample(), {0}, {1, 0}, centreZone());
        QVERIFY(r.path == NodePath({0, 0, 1}));
        QCOMPARE(itemAt(r.tree, r.path), QStringLiteral("a"));
        QCOMPARE(itemAt(r.tree, {0, 0, 0}), QStringLiteral("b"));

        // A tab moved out of a tab group keeps its title.
        r = layout::moveNode(sample(), {2, 0}, {0}, centreZone());
        QVERIFY(r.path == NodePath({0, 1}));
        QVERIFY(layout::nodeAt(r.tree, {0})->titles == std::vector<QString>({QString(), "D"}));

        // Into a cell in a tab group: after that cell.
        r = layout::moveNode(sample(), {1, 1}, {2, 0}, centreZone());
        QVERIFY(r.path == NodePath({2, 1}));
        QVERIFY(layout::nodeAt(r.tree, {2})->titles == std::vector<QString>({"D", QString(), "E"}));
    }

    void moveToTabBar()
    {
        EditResult r = layout::moveNode(sample(), {0}, {2}, tabBarZone(1));
        QVERIFY(r.path == NodePath({1, 1}));
        QCOMPARE(itemAt(r.tree, r.path), QStringLiteral("a"));
        QCOMPARE(layout::nodeAt(r.tree, {1})->current, 1);

        // Within the same tab bar, the index counts the tab being moved.
        LayoutNode tree = sample();
        tree.children[2] = tabs({cell("d"), cell("e"), cell("f")}, {"D", "E", "F"});
        r = layout::moveNode(tree, {2, 0}, {2}, tabBarZone(3));
        QVERIFY(r.path == NodePath({2, 2}));
        QVERIFY(layout::nodeAt(r.tree, {2})->titles == std::vector<QString>({"E", "F", "D"}));

        r = layout::moveNode(tree, {2, 2}, {2}, tabBarZone(0));
        QVERIFY(r.path == NodePath({2, 0}));
        QVERIFY(layout::nodeAt(r.tree, {2})->titles == std::vector<QString>({"F", "D", "E"}));

        // Not a tab group, or index out of range.
        QVERIFY(layout::moveNode(tree, {0}, {1}, tabBarZone(0)).path.empty());
        QVERIFY(layout::moveNode(tree, {0}, {2}, tabBarZone(4)).path.empty());
    }

    void moveRejected()
    {
        const LayoutNode tree = sample();
        const struct { NodePath from, to; } cases[] = {
            {{1}, {1}}, {{1}, {1, 0}}, {{}, {0}}, {{5}, {0}}, {{0}, {9, 9}}};
        for (const auto &c : cases) {
            const EditResult r = layout::moveNode(tree, c.from, c.to, edgeZone(Edge::Left));
            QVERIFY(r.path.empty());
            QVERIFY(layout::sameShape(r.tree, tree));
        }
    }

    void renameTab()
    {
        const LayoutNode r = layout::renameTab(sample(), {2}, 1, "X");
        QVERIFY(r.children[2].titles == std::vector<QString>({"D", "X"}));
        QVERIFY(layout::sameShape(layout::renameTab(sample(), {2}, 2, "X"), sample()));
    }

    void moveTab()
    {
        LayoutNode tree = sample();
        tree.children[2] = tabs({cell("d"), cell("e"), cell("f")}, {"D", "E", "F"}, 0);

        LayoutNode r = layout::moveTab(tree, {2}, 0, 2);
        QVERIFY(r.children[2].titles == std::vector<QString>({"E", "F", "D"}));
        QCOMPARE(itemAt(r, {2, 2}), QStringLiteral("d"));
        QCOMPARE(r.children[2].current, 2);   // the current tab moved with it

        tree.children[2].current = 1;
        r = layout::moveTab(tree, {2}, 0, 2);
        QCOMPARE(r.children[2].current, 0);   // e is still current
        r = layout::moveTab(tree, {2}, 2, 0);
        QCOMPARE(r.children[2].current, 2);
        r = layout::moveTab(tree, {2}, 1, 1);
        QVERIFY(r.children[2].titles == std::vector<QString>({"D", "E", "F"}));

        QVERIFY(layout::sameShape(layout::moveTab(tree, {2}, 0, 3), tree));
    }

    void setCurrentTab()
    {
        QCOMPARE(layout::setCurrentTab(sample(), {2}, 0).children[2].current, 0);
        QCOMPARE(layout::setCurrentTab(sample(), {2}, 2).children[2].current, 1);
        QCOMPARE(layout::setCurrentTab(sample(), {2}, -1).children[2].current, 1);
    }

    void setCollapseSingle()
    {
        QVERIFY(layout::setCollapseSingle(sample(), {2}, true).children[2].collapseSingle);
        QVERIFY(!layout::setCollapseSingle(sample(), {0}, true).children[0].collapseSingle);
    }

    void setSizes()
    {
        const LayoutNode r = layout::setSizes(sample(), {}, {1, 2, 3});
        QVERIFY(r.sizes == std::vector<double>({1, 2, 3}));
        QVERIFY(layout::setSizes(sample(), {}, {1, 2}).sizes == std::vector<double>({1, 1, 1}));
        QVERIFY(layout::setSizes(sample(), {2}, {1, 2}).sizes == std::vector<double>({1, 1, 1}));
    }

    void outOfRangeLeavesTreeUnchanged()
    {
        const LayoutNode tree = sample();
        const NodePath bad = {7};
        QVERIFY(layout::splitCell(tree, bad, Edge::Left, std::nullopt).path.empty());
        QVERIFY(layout::wrapInTabs(tree, bad, "T").path.empty());
        QVERIFY(layout::addTab(tree, bad, std::nullopt, "T").path.empty());
        QVERIFY(layout::sameShape(layout::splitCell(tree, bad, Edge::Left, std::nullopt).tree, tree));
        QVERIFY(layout::sameShape(layout::removeNode(tree, bad), tree));
        QVERIFY(layout::sameShape(layout::setRef(tree, bad, item("x")), tree));
        QVERIFY(layout::sameShape(layout::renameTab(tree, bad, 0, "x"), tree));
    }

    void sameShapeIgnoresSizesCurrentAndExtra()
    {
        LayoutNode other = sample();
        other.sizes = {3, 2, 1};
        other.children[2].current = 0;
        other.extra.insert("future", 1);
        QVERIFY(layout::sameShape(sample(), other));
    }

    void sameShapeSeesStructure()
    {
        const LayoutNode base = sample();
        auto differs = [&](const std::function<void(LayoutNode &)> &change) {
            LayoutNode other = base;
            change(other);
            return !layout::sameShape(base, other);
        };
        QVERIFY(differs([](LayoutNode &t) { t.dir = Qt::Vertical; }));
        QVERIFY(differs([](LayoutNode &t) { t.children.pop_back(); t.sizes.pop_back(); }));
        QVERIFY(differs([](LayoutNode &t) { t.children[0].ref = item("z"); }));
        QVERIFY(differs([](LayoutNode &t) { t.children[0].ref.reset(); }));
        QVERIFY(differs([](LayoutNode &t) { t.children[0].removed = true; }));
        QVERIFY(differs([](LayoutNode &t) { t.children[2].titles[0] = "Z"; }));
        QVERIFY(differs([](LayoutNode &t) { t.children[2].collapseSingle = true; }));
        QVERIFY(differs([](LayoutNode &t) { t.children[1].kind = Kind::Tabs; }));
        QVERIFY(differs([](LayoutNode &t) { t.children[1].children[1].ref->task = "t"; }));
    }
};

QTEST_GUILESS_MAIN(TestLayoutEdits)
#include "tst_layout_edits.moc"
