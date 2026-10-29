// Tests the layout presets: their names, their shapes, and how items fill them.
#include "layout/Presets.h"

#include <QTest>

using namespace mirante;

namespace {

using Kind = LayoutNode::Kind;

QStringList ids(int count)
{
    QStringList result;
    for (int i = 0; i < count; ++i)
        result << QStringLiteral("item%1").arg(i);
    return result;
}

// The item of each cell in depth-first order, or an empty string for an empty cell.
QStringList cellItems(const LayoutNode &tree)
{
    QStringList result;
    for (const NodePath &path : layout::cellPaths(tree)) {
        const LayoutNode *node = layout::nodeAt(tree, path);
        result << (node->ref ? node->ref->item : QString());
    }
    return result;
}

// A preset tree is already normal: normalising changes nothing.
bool isNormal(const LayoutNode &tree)
{
    const LayoutNode normal = layout::normalise(tree, [](const CellRef &) { return true; });
    return layout::sameShape(normal, tree) && layoutToJson(normal) == layoutToJson(tree);
}

} // namespace

class TestPresets : public QObject
{
    Q_OBJECT

private slots:
    void names()
    {
        QCOMPARE(presets::names(),
                 (QStringList{QStringLiteral("Full"), QStringLiteral("Two side by side"),
                              QStringLiteral("Two stacked"), QStringLiteral("Grid of four"),
                              QStringLiteral("One large on top, two below"),
                              QStringLiteral("One large on the left, two stacked on the right"),
                              QStringLiteral("Tabs")}));
    }

    void cellCounts_data()
    {
        QTest::addColumn<int>("index");
        QTest::addColumn<int>("cells");
        QTest::newRow("full") << 0 << 1;
        QTest::newRow("side by side") << 1 << 2;
        QTest::newRow("stacked") << 2 << 2;
        QTest::newRow("grid") << 3 << 4;
        QTest::newRow("large on top") << 4 << 3;
        QTest::newRow("large on the left") << 5 << 3;
        QTest::newRow("tabs") << 6 << 1;
    }

    void cellCounts()
    {
        QFETCH(int, index);
        QFETCH(int, cells);

        const LayoutNode empty = presets::build(index);
        QCOMPARE(int(layout::cellPaths(empty).size()), cells);
        QCOMPARE(cellItems(empty), QStringList(cells, QString()));
        QVERIFY(isNormal(empty));

        // Items fill the cells in order; the rest stay empty.
        const LayoutNode partial = presets::build(index, ids(1));
        QStringList expected(cells, QString());
        expected[0] = QStringLiteral("item0");
        QCOMPARE(cellItems(partial), expected);

        if (index != 6) {
            // Items beyond the cells are left out.
            const LayoutNode full = presets::build(index, ids(cells + 2));
            QCOMPARE(cellItems(full), ids(cells));
            QVERIFY(isNormal(full));
        }
    }

    void shapes()
    {
        const LayoutNode full = presets::build(0);
        QCOMPARE(full.kind, Kind::Cell);

        const LayoutNode side = presets::build(1);
        QCOMPARE(side.kind, Kind::Split);
        QCOMPARE(side.dir, Qt::Horizontal);
        QCOMPARE(side.sizes, (std::vector<double>{1.0, 1.0}));

        const LayoutNode stacked = presets::build(2);
        QCOMPARE(stacked.kind, Kind::Split);
        QCOMPARE(stacked.dir, Qt::Vertical);

        const LayoutNode grid = presets::build(3);
        QCOMPARE(grid.dir, Qt::Vertical);
        QCOMPARE(grid.children.size(), std::size_t(2));
        for (const LayoutNode &row : grid.children) {
            QCOMPARE(row.kind, Kind::Split);
            QCOMPARE(row.dir, Qt::Horizontal);
            QCOMPARE(row.children.size(), std::size_t(2));
        }

        const LayoutNode top = presets::build(4, ids(3));
        QCOMPARE(top.dir, Qt::Vertical);
        QCOMPARE(top.sizes, (std::vector<double>{2.0, 1.0}));
        QCOMPARE(top.children[0].kind, Kind::Cell);
        QCOMPARE(top.children[0].ref->item, QStringLiteral("item0"));
        QCOMPARE(top.children[1].dir, Qt::Horizontal);

        const LayoutNode left = presets::build(5, ids(3));
        QCOMPARE(left.dir, Qt::Horizontal);
        QCOMPARE(left.sizes, (std::vector<double>{2.0, 1.0}));
        QCOMPARE(left.children[0].ref->item, QStringLiteral("item0"));
        QCOMPARE(left.children[1].dir, Qt::Vertical);
    }

    void tabs()
    {
        const LayoutNode tabs = presets::build(6, ids(3), {QStringLiteral("A"), QStringLiteral("B")});
        QCOMPARE(tabs.kind, Kind::Tabs);
        QCOMPARE(cellItems(tabs), ids(3));
        QCOMPARE(tabs.titles, (std::vector<QString>{QStringLiteral("A"), QStringLiteral("B"), QString()}));
        QCOMPARE(tabs.current, 0);
        QVERIFY(isNormal(tabs));

        const LayoutNode empty = presets::build(6);
        QCOMPARE(empty.kind, Kind::Tabs);
        QCOMPARE(empty.children.size(), std::size_t(1));
        QCOMPARE(empty.titles.size(), std::size_t(1));
        QVERIFY(!empty.children[0].ref);
    }

    void unknownIndex()
    {
        for (int index : {-1, 7, 100}) {
            const LayoutNode tree = presets::build(index, ids(2));
            QCOMPARE(tree.kind, Kind::Cell);
            QVERIFY(!tree.ref);
        }
    }
};

QTEST_GUILESS_MAIN(TestPresets)
#include "tst_presets.moc"
