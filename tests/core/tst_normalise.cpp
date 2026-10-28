// Tests layout::normalise(): each rule, combinations, size scaling, validity
// fixes, idempotence and path tracking.
#include "core/LayoutTree.h"

#include <QTest>

#include <cmath>
#include <cstddef>
#include <utility>

using namespace mirante;

namespace {

LayoutNode cell(const QString &item)
{
    LayoutNode node;
    node.ref = CellRef{item, QString(), QString()};
    return node;
}

LayoutNode empty()
{
    return LayoutNode();
}

LayoutNode split(Qt::Orientation dir, std::vector<LayoutNode> children, std::vector<double> sizes = {})
{
    LayoutNode node;
    node.kind = LayoutNode::Kind::Split;
    node.dir = dir;
    if (sizes.empty())
        sizes.assign(children.size(), 1.0);
    node.sizes = std::move(sizes);
    node.children = std::move(children);
    return node;
}

LayoutNode hsplit(std::vector<LayoutNode> children, std::vector<double> sizes = {})
{
    return split(Qt::Horizontal, std::move(children), std::move(sizes));
}

LayoutNode vsplit(std::vector<LayoutNode> children, std::vector<double> sizes = {})
{
    return split(Qt::Vertical, std::move(children), std::move(sizes));
}

LayoutNode tabs(std::vector<LayoutNode> children, bool collapseSingle = false, int current = 0)
{
    LayoutNode node;
    node.kind = LayoutNode::Kind::Tabs;
    for (std::size_t i = 0; i < children.size(); ++i)
        node.titles.push_back(QStringLiteral("t%1").arg(i));
    node.children = std::move(children);
    node.collapseSingle = collapseSingle;
    node.current = current;
    return node;
}

// A text form of a tree, with structure, sizes, titles and current tab.
// h[1:a,2:-] is a horizontal split of cell a and an empty cell.
QString describe(const LayoutNode &node)
{
    QString text;
    if (node.kind == LayoutNode::Kind::Cell) {
        if (node.ref)
            return node.ref->item;
        return node.removed ? QStringLiteral("removed") : QStringLiteral("-");
    }
    if (node.kind == LayoutNode::Kind::Split) {
        text = node.dir == Qt::Horizontal ? QStringLiteral("h[") : QStringLiteral("v[");
        for (std::size_t i = 0; i < node.children.size(); ++i) {
            if (i > 0)
                text += QLatin1Char(',');
            const QString size = i < node.sizes.size() ? QString::number(node.sizes[i], 'g', 6)
                                                       : QStringLiteral("?");
            text += size + QLatin1Char(':') + describe(node.children[i]);
        }
        return text + QLatin1Char(']');
    }
    text = QStringLiteral("tabs%1%2[").arg(node.current).arg(node.collapseSingle ? "c" : "");
    for (std::size_t i = 0; i < node.children.size(); ++i) {
        if (i > 0)
            text += QLatin1Char(',');
        const QString title = i < node.titles.size() ? node.titles[i] : QStringLiteral("?");
        text += title + QLatin1Char('=') + describe(node.children[i]);
    }
    if (node.titles.size() > node.children.size())
        text += QStringLiteral("+titles");
    return text + QLatin1Char(']');
}

const RefExists allExist;

RefExists allExcept(const QString &gone)
{
    return [gone](const CellRef &ref) { return ref.item != gone; };
}

QString normalised(const LayoutNode &tree, const RefExists &exists = allExist)
{
    return describe(layout::normalise(tree, exists));
}

// The node at path. path must be valid.
const LayoutNode &at(const LayoutNode &tree, const NodePath &path)
{
    const LayoutNode *node = &tree;
    for (int index : path)
        node = &node->children[static_cast<std::size_t>(index)];
    return *node;
}

std::optional<NodePath> tracked(const NodePath &path)
{
    return path;
}

} // namespace

Q_DECLARE_METATYPE(LayoutNode)

class TestNormalise : public QObject
{
    Q_OBJECT

private slots:
    void validTreeIsUnchanged()
    {
        const LayoutNode tree = hsplit({cell("a"), vsplit({cell("b"), tabs({cell("c"), empty()})})}, {1, 2});
        QCOMPARE(normalised(tree), describe(tree));
    }

    void rule1SingleChildSplit()
    {
        QCOMPARE(normalised(hsplit({cell("a")})), QStringLiteral("a"));
        QCOMPARE(normalised(hsplit({cell("a"), vsplit({cell("b")})})), QStringLiteral("h[1:a,1:b]"));
    }

    void rule1KeepsChildExtra()
    {
        LayoutNode child = cell("a");
        child.extra.insert(QStringLiteral("child"), 1);
        LayoutNode wrapper = hsplit({child});
        wrapper.extra.insert(QStringLiteral("wrapper"), 2);
        const LayoutNode result = layout::normalise(wrapper, allExist);
        QCOMPARE(result.extra.value(QStringLiteral("child")).toInt(), 1);
        QVERIFY(!result.extra.contains(QStringLiteral("wrapper")));
    }

    void rule2SameDirection()
    {
        // The child had half the space, so its 1:3 sizes become 0.25 and 0.75.
        const LayoutNode tree = hsplit({cell("a"), hsplit({cell("b"), cell("c")}, {1, 3})}, {1, 1});
        QCOMPARE(normalised(tree), QStringLiteral("h[1:a,0.25:b,0.75:c]"));
    }

    void rule2ScalesToChildShare()
    {
        // The child had 3 of 4 parts; its sizes 2 and 2 become 1.5 and 1.5.
        const LayoutNode tree = vsplit({vsplit({cell("a"), cell("b")}, {2, 2}), cell("c")}, {3, 1});
        QCOMPARE(normalised(tree), QStringLiteral("v[1.5:a,1.5:b,1:c]"));
    }

    void rule2Nested()
    {
        const LayoutNode tree = hsplit({hsplit({hsplit({cell("a"), cell("b")}), cell("c")}), cell("d")});
        QCOMPARE(normalised(tree), QStringLiteral("h[0.25:a,0.25:b,0.5:c,1:d]"));
    }

    void rule2OtherDirectionKept()
    {
        const LayoutNode tree = hsplit({cell("a"), vsplit({cell("b"), cell("c")})});
        QCOMPARE(normalised(tree), describe(tree));
    }

    void rule3EmptyTabs()
    {
        QCOMPARE(normalised(tabs({})), QStringLiteral("-"));
        QCOMPARE(normalised(hsplit({cell("a"), tabs({})})), QStringLiteral("h[1:a,1:-]"));
    }

    void rule4SingleTab()
    {
        QCOMPARE(normalised(tabs({cell("a")})), QStringLiteral("tabs0[t0=a]"));
        QCOMPARE(normalised(tabs({cell("a")}, true)), QStringLiteral("a"));
        QCOMPARE(normalised(tabs({cell("a"), cell("b")}, true)), QStringLiteral("tabs0c[t0=a,t1=b]"));
    }

    void rule7SingleTabAroundTabs()
    {
        // The inner group replaces the outer one, at any depth of nesting.
        QCOMPARE(normalised(tabs({tabs({cell("a"), cell("b")})})), QStringLiteral("tabs0[t0=a,t1=b]"));
        QCOMPARE(normalised(tabs({tabs({tabs({cell("a")})})})), QStringLiteral("tabs0[t0=a]"));
        QCOMPARE(normalised(hsplit({cell("x"), tabs({tabs({cell("a")})})})), QStringLiteral("h[1:x,1:tabs0[t0=a]]"));
        // Tabs inside one of several tabs stay: they add a level the user can see.
        const LayoutNode kept = tabs({tabs({cell("a"), cell("b")}), cell("c")});
        QCOMPARE(normalised(kept), describe(kept));
    }

    void rule5EmptyCellsKept()
    {
        QCOMPARE(normalised(hsplit({empty(), empty()})), QStringLiteral("h[1:-,1:-]"));
        QCOMPARE(normalised(tabs({empty()})), QStringLiteral("tabs0[t0=-]"));
        QCOMPARE(normalised(empty()), QStringLiteral("-"));
    }

    void rule6DanglingRef()
    {
        const LayoutNode tree = hsplit({cell("a"), cell("gone")});
        const LayoutNode result = layout::normalise(tree, allExcept(QStringLiteral("gone")));
        QCOMPARE(describe(result), QStringLiteral("h[1:a,1:removed]"));
        QVERIFY(!result.children[1].ref);
        QVERIFY(result.children[1].removed);
        QVERIFY(!result.children[0].removed);
    }

    void rule6EmptyExistsKeepsAll()
    {
        const LayoutNode tree = hsplit({cell("a"), cell("gone")});
        QCOMPARE(normalised(tree, RefExists()), describe(tree));
    }

    void rule6EmptyCellNotRemoved()
    {
        const LayoutNode result = layout::normalise(empty(), allExcept(QStringLiteral("a")));
        QVERIFY(!result.removed);
    }

    void splitWithoutChildren()
    {
        QCOMPARE(normalised(hsplit({})), QStringLiteral("-"));
        QCOMPARE(normalised(vsplit({cell("a"), hsplit({})})), QStringLiteral("v[1:a,1:-]"));
    }

    void badSizesBecomeEqual()
    {
        QCOMPARE(normalised(hsplit({cell("a"), cell("b")}, {1})), QStringLiteral("h[1:a,1:b]"));
        QCOMPARE(normalised(hsplit({cell("a"), cell("b")}, {1, 2, 3})), QStringLiteral("h[1:a,1:b]"));
        QCOMPARE(normalised(hsplit({cell("a"), cell("b")}, {2, 0})), QStringLiteral("h[1:a,1:b]"));
        QCOMPARE(normalised(hsplit({cell("a"), cell("b")}, {-1, 2})), QStringLiteral("h[1:a,1:b]"));
        QCOMPARE(normalised(hsplit({cell("a"), cell("b")}, {std::nan(""), 2})), QStringLiteral("h[1:a,1:b]"));
        QCOMPARE(normalised(hsplit({cell("a"), cell("b")}, {0.3, 0.7})), QStringLiteral("h[0.3:a,0.7:b]"));
    }

    void badSizesInAbsorbedChild()
    {
        // The child's sizes are fixed before it is absorbed.
        const LayoutNode tree = hsplit({cell("a"), hsplit({cell("b"), cell("c")}, {0, 5})}, {1, 1});
        QCOMPARE(normalised(tree), QStringLiteral("h[1:a,0.5:b,0.5:c]"));
    }

    void currentTabClamped()
    {
        QCOMPARE(normalised(tabs({cell("a"), cell("b")}, false, -1)), QStringLiteral("tabs0[t0=a,t1=b]"));
        QCOMPARE(normalised(tabs({cell("a"), cell("b")}, false, 5)), QStringLiteral("tabs1[t0=a,t1=b]"));
        QCOMPARE(normalised(tabs({cell("a"), cell("b")}, false, 1)), QStringLiteral("tabs1[t0=a,t1=b]"));
    }

    void titlesMatchChildren()
    {
        LayoutNode few = tabs({cell("a"), cell("b"), cell("c")});
        few.titles = {QStringLiteral("x")};
        QCOMPARE(normalised(few), QStringLiteral("tabs0[x=a,=b,=c]"));

        LayoutNode many = tabs({cell("a")});
        many.titles = {QStringLiteral("x"), QStringLiteral("y")};
        QCOMPARE(normalised(many), QStringLiteral("tabs0[x=a]"));
    }

    void combinations()
    {
        // A collapsing tab group holding a same-direction split: rule 4, then rule 2.
        QCOMPARE(normalised(hsplit({cell("a"), tabs({hsplit({cell("b"), cell("c")})}, true)})),
                 QStringLiteral("h[1:a,0.5:b,0.5:c]"));
        // A single-child split of the other direction around a same-direction split: rule 1, then rule 2.
        QCOMPARE(normalised(vsplit({cell("x"), hsplit({vsplit({cell("a"), cell("b")})})})),
                 QStringLiteral("v[1:x,0.5:a,0.5:b]"));
        // Empty tabs inside a single-child split inside a split: rules 3 and 1.
        QCOMPARE(normalised(hsplit({cell("a"), vsplit({tabs({})})})), QStringLiteral("h[1:a,1:-]"));
        // Everything collapses to one removed cell.
        QCOMPARE(normalised(hsplit({vsplit({tabs({hsplit({cell("gone")})}, true)})}),
                            allExcept(QStringLiteral("gone"))),
                 QStringLiteral("removed"));
    }

    void idempotent_data()
    {
        QTest::addColumn<LayoutNode>("tree");
        QTest::newRow("valid") << hsplit({cell("a"), vsplit({cell("b"), cell("c")})}, {1, 2});
        QTest::newRow("merge") << hsplit({hsplit({cell("a"), cell("b")}, {1, 2}), hsplit({cell("c"), cell("d")}, {3, 7})}, {0.3, 0.7});
        QTest::newRow("deep") << vsplit({hsplit({vsplit({tabs({vsplit({cell("a"), cell("b")})}, true)})}), cell("c")});
        QTest::newRow("invalid") << hsplit({tabs({cell("a"), cell("b")}, false, 9), hsplit({}), cell("gone")}, {0});
        QTest::newRow("single") << tabs({hsplit({cell("a")})});
    }

    void idempotent()
    {
        QFETCH(LayoutNode, tree);
        const RefExists exists = allExcept(QStringLiteral("gone"));
        const LayoutNode once = layout::normalise(tree, exists);
        const LayoutNode twice = layout::normalise(once, exists);
        QCOMPARE(describe(twice), describe(once));
    }

    void trackNoChange()
    {
        const LayoutNode tree = hsplit({cell("a"), vsplit({cell("b"), cell("c")})});
        std::optional<NodePath> track = NodePath{1, 0};
        layout::normalise(tree, allExist, &track);
        QCOMPARE(track, tracked({1, 0}));
    }

    void trackThroughMerge()
    {
        // h[a, h[b, v[c, d]], e]: the inner h is absorbed, so d moves from 1/1/1 to 2/1.
        const LayoutNode tree = hsplit({cell("a"), hsplit({cell("b"), vsplit({cell("c"), cell("d")})}), cell("e")});
        std::optional<NodePath> track = NodePath{1, 1, 1};
        const LayoutNode result = layout::normalise(tree, allExist, &track);
        QCOMPARE(track, tracked({2, 1}));
        QCOMPARE(at(result, *track).ref->item, QStringLiteral("d"));

        // A sibling after the absorbed split shifts right.
        track = NodePath{2};
        layout::normalise(tree, allExist, &track);
        QCOMPARE(track, tracked({3}));

        // A sibling before it keeps its index.
        track = NodePath{0};
        layout::normalise(tree, allExist, &track);
        QCOMPARE(track, tracked({0}));
    }

    void trackAbsorbedSplitCleared()
    {
        const LayoutNode tree = hsplit({cell("a"), hsplit({cell("b"), cell("c")}), cell("d")});
        std::optional<NodePath> track = NodePath{1};
        layout::normalise(tree, allExist, &track);
        QVERIFY(!track);
    }

    void trackWrapperCleared()
    {
        const LayoutNode tree = vsplit({cell("a"), hsplit({cell("b")})});
        std::optional<NodePath> track = NodePath{1};
        layout::normalise(tree, allExist, &track);
        QVERIFY(!track);

        const LayoutNode tabTree = vsplit({cell("a"), tabs({cell("b")}, true)});
        track = NodePath{1};
        layout::normalise(tabTree, allExist, &track);
        QVERIFY(!track);
    }

    void trackChildOfWrapperFollows()
    {
        // v[a, h[b]]: b moves from 1/0 to 1.
        const LayoutNode tree = vsplit({cell("a"), hsplit({cell("b")})});
        std::optional<NodePath> track = NodePath{1, 0};
        layout::normalise(tree, allExist, &track);
        QCOMPARE(track, tracked({1}));

        // v[a, tabs(collapse)[h[b, c]]]: c moves from 1/0/1 to 1/1.
        const LayoutNode tabTree = vsplit({cell("a"), tabs({hsplit({cell("b"), cell("c")})}, true)});
        track = NodePath{1, 0, 1};
        const LayoutNode result = layout::normalise(tabTree, allExist, &track);
        QCOMPARE(track, tracked({1, 1}));
        QCOMPARE(at(result, *track).ref->item, QStringLiteral("c"));
    }

    void trackChildOfWrapperThenMerged()
    {
        // h[a, v[h[b, c]]]: the v wrapper goes (rule 1), then its h child is absorbed (rule 2).
        const LayoutNode tree = hsplit({cell("a"), vsplit({hsplit({cell("b"), cell("c")})})});
        std::optional<NodePath> track = NodePath{1, 0, 1};
        const LayoutNode result = layout::normalise(tree, allExist, &track);
        QCOMPARE(describe(result), QStringLiteral("h[1:a,0.5:b,0.5:c]"));
        QCOMPARE(track, tracked({2}));

        track = NodePath{1, 0};
        layout::normalise(tree, allExist, &track);
        QVERIFY(!track);
    }

    void trackRootWrapperChild()
    {
        // h[v[a, b]]: the root is replaced by v, so b moves from 0/1 to 1.
        const LayoutNode tree = hsplit({vsplit({cell("a"), cell("b")})});
        std::optional<NodePath> track = NodePath{0, 1};
        layout::normalise(tree, allExist, &track);
        QCOMPARE(track, tracked({1}));
    }

    void trackRootReplaced()
    {
        // h[v[a, b]]: the root h is replaced by its only child v.
        const LayoutNode tree = hsplit({vsplit({cell("a"), cell("b")})});
        std::optional<NodePath> track = NodePath{};
        layout::normalise(tree, allExist, &track);
        QVERIFY(!track);

        track = NodePath{0};
        const LayoutNode result = layout::normalise(tree, allExist, &track);
        QCOMPARE(track, tracked({}));
        QCOMPARE(describe(at(result, *track)), QStringLiteral("v[1:a,1:b]"));

        // The root kept in place is still the root.
        track = NodePath{};
        layout::normalise(hsplit({cell("a"), cell("b")}), allExist, &track);
        QCOMPARE(track, tracked({}));
    }

    void trackNullopt()
    {
        std::optional<NodePath> track;
        layout::normalise(hsplit({cell("a")}), allExist, &track);
        QVERIFY(!track);
    }

    void trackKeptInPlace()
    {
        // A cell whose ref is gone stays in place.
        const LayoutNode tree = hsplit({cell("a"), cell("gone")});
        std::optional<NodePath> track = NodePath{1};
        layout::normalise(tree, allExcept(QStringLiteral("gone")), &track);
        QCOMPARE(track, tracked({1}));

        // An empty tab group becomes an empty cell in the same place.
        const LayoutNode tabTree = hsplit({cell("a"), tabs({})});
        track = NodePath{1};
        layout::normalise(tabTree, allExist, &track);
        QCOMPARE(track, tracked({1}));
    }

    void trackInvalidPathCleared()
    {
        const LayoutNode tree = hsplit({cell("a"), cell("b")});
        std::optional<NodePath> track = NodePath{5};
        layout::normalise(tree, allExist, &track);
        QVERIFY(!track);

        track = NodePath{0, 0};
        layout::normalise(tree, allExist, &track);
        QVERIFY(!track);
    }
};


QTEST_GUILESS_MAIN(TestNormalise)
#include "tst_normalise.moc"
