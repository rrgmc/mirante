// JSON conversion of layout trees and cell references.
#include "core/LayoutTree.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QTest>

using namespace mirante;

namespace {

QJsonObject parse(const char *text)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(QByteArray(text), &error);
    if (error.error != QJsonParseError::NoError)
        qFatal("bad test JSON: %s", qPrintable(error.errorString()));
    return document.object();
}

// The example of docs/data-model.md#layout-tree.
const char *const DocExample = R"({
  "type": "split", "dir": "h", "sizes": [2, 1],
  "children": [
    { "type": "cell", "ref": { "item": "claude-1" } },
    { "type": "tabs", "current": 0, "collapseSingle": false,
      "tabs": [
        { "title": "Issues", "node": {
            "type": "split", "dir": "v", "sizes": [1, 1],
            "children": [
              { "type": "cell", "ref": { "item": "jira-1" } },
              { "type": "cell", "ref": { "item": "jira-2" } }
            ] } },
        { "title": "MR", "node": { "type": "cell", "ref": { "item": "mr-1" } } }
      ] }
  ]
})";

} // namespace

class TestLayoutJson : public QObject
{
    Q_OBJECT

private slots:
    void cellRefKeys()
    {
        const CellRef item = cellRefFromJson(parse(R"({ "item": "i-1" })"));
        QVERIFY(item.isItem());
        QCOMPARE(item.item, QStringLiteral("i-1"));
        QCOMPARE(cellRefToJson(item), parse(R"({ "item": "i-1" })"));

        const CellRef task = cellRefFromJson(parse(R"({ "task": "t-1", "dashboard": "d-1" })"));
        QVERIFY(task.isTask());
        QCOMPARE(cellRefToJson(task), parse(R"({ "task": "t-1", "dashboard": "d-1" })"));

        const CellRef both = cellRefFromJson(parse(R"({ "item": "i-1", "task": "t-1" })"));
        QVERIFY(both.isItem());
        QCOMPARE(cellRefToJson(both), parse(R"({ "item": "i-1", "task": "t-1" })"));

        QCOMPARE(cellRefToJson(CellRef{}), QJsonObject());
        // Values that are not strings are not set.
        QCOMPARE(cellRefFromJson(parse(R"({ "item": 4, "task": null })")), CellRef{});
    }

    void docExample()
    {
        const QJsonObject json = parse(DocExample);
        const LayoutNode tree = layoutFromJson(json);
        QCOMPARE(tree.kind, LayoutNode::Kind::Split);
        QCOMPARE(tree.dir, Qt::Horizontal);
        QCOMPARE(tree.sizes, (std::vector<double>{2, 1}));
        QCOMPARE(tree.children.size(), std::size_t(2));
        QCOMPARE(tree.children[0].ref->item, QStringLiteral("claude-1"));

        const LayoutNode &tabs = tree.children[1];
        QCOMPARE(tabs.kind, LayoutNode::Kind::Tabs);
        QCOMPARE(tabs.titles, (std::vector<QString>{QStringLiteral("Issues"), QStringLiteral("MR")}));
        QCOMPARE(tabs.current, 0);
        QVERIFY(!tabs.collapseSingle);
        QCOMPARE(tabs.children[0].kind, LayoutNode::Kind::Split);
        QCOMPARE(tabs.children[0].dir, Qt::Vertical);
        QCOMPARE(tabs.children[0].children[1].ref->item, QStringLiteral("jira-2"));
        QCOMPARE(tabs.children[1].ref->item, QStringLiteral("mr-1"));
        QVERIFY(tree.extra.isEmpty());
        QVERIFY(tabs.extra.isEmpty());

        QCOMPARE(layoutToJson(tree), json);
    }

    void emptyCell()
    {
        const LayoutNode cell = layoutFromJson(parse(R"({ "type": "cell", "ref": null })"));
        QCOMPARE(cell.kind, LayoutNode::Kind::Cell);
        QVERIFY(!cell.ref);
        QCOMPARE(layoutToJson(cell), parse(R"({ "type": "cell", "ref": null })"));
        QCOMPARE(layoutToJson(LayoutNode{}), parse(R"({ "type": "cell", "ref": null })"));

        // A missing, empty or unusable ref is an empty cell too.
        QVERIFY(!layoutFromJson(parse(R"({ "type": "cell" })")).ref);
        QVERIFY(!layoutFromJson(parse(R"({ "type": "cell", "ref": {} })")).ref);
        QVERIFY(!layoutFromJson(parse(R"({ "type": "cell", "ref": "i-1" })")).ref);
    }

    void removedNeverWritten()
    {
        LayoutNode cell;
        cell.removed = true;
        QCOMPARE(layoutToJson(cell), parse(R"({ "type": "cell", "ref": null })"));
    }

    void unknownKeysKept()
    {
        const QJsonObject json = parse(R"({ "type": "split", "dir": "v", "sizes": [1, 2],
            "locked": true, "children": [
                { "type": "cell", "ref": { "item": "a" }, "zoom": 1.5 },
                { "type": "tabs", "current": 0, "collapseSingle": true, "style": "pills",
                  "tabs": [ { "title": "x", "node": { "type": "cell", "ref": null } } ] } ] })");
        const LayoutNode tree = layoutFromJson(json);
        QCOMPARE(tree.extra, parse(R"({ "locked": true })"));
        QCOMPARE(tree.children[0].extra, parse(R"({ "zoom": 1.5 })"));
        QCOMPARE(tree.children[1].extra, parse(R"({ "style": "pills" })"));
        QCOMPARE(layoutToJson(tree), json);
    }

    void unknownNodeType()
    {
        // An unknown node is an empty cell that is written back unchanged.
        const QJsonObject json = parse(R"({ "type": "split", "dir": "h", "sizes": [1, 1],
            "children": [ { "type": "cell", "ref": null },
                          { "type": "grid", "rows": [ { "type": "cell" } ] } ] })");
        const LayoutNode tree = layoutFromJson(json);
        const LayoutNode &grid = tree.children[1];
        QCOMPARE(grid.kind, LayoutNode::Kind::Cell);
        QVERIFY(!grid.ref);
        QCOMPARE(layoutToJson(tree), json);

        // Once an edit gives it a ref, it is a plain cell.
        LayoutNode edited = grid;
        edited.ref = CellRef{QStringLiteral("i-1"), QString(), QString()};
        QCOMPARE(layoutToJson(edited), parse(R"({ "type": "cell", "ref": { "item": "i-1" } })"));

        // A node without a type is an empty cell that keeps its other keys.
        const LayoutNode untyped = layoutFromJson(parse(R"({ "dir": "h" })"));
        QCOMPARE(untyped.kind, LayoutNode::Kind::Cell);
        QVERIFY(!untyped.ref);
        QCOMPARE(layoutToJson(untyped), parse(R"({ "type": "cell", "ref": null, "dir": "h" })"));
    }

    void splitRepairs()
    {
        // Sizes that do not fit the children become equal.
        const LayoutNode wrongCount = layoutFromJson(parse(R"({ "type": "split", "dir": "h",
            "sizes": [1], "children": [ { "type": "cell" }, { "type": "cell" } ] })"));
        QCOMPARE(wrongCount.sizes, (std::vector<double>{1, 1}));

        const LayoutNode badValue = layoutFromJson(parse(R"({ "type": "split", "dir": "h",
            "sizes": [1, -2, "x"], "children": [ {"type": "cell"}, {"type": "cell"}, {"type": "cell"} ] })"));
        QCOMPARE(badValue.sizes, (std::vector<double>{1, 1, 1}));
        QVERIFY(badValue.extra.isEmpty());

        // A missing dir is "h", and children that are not objects are empty cells.
        const LayoutNode noDir = layoutFromJson(parse(R"({ "type": "split",
            "children": [ 5, { "type": "cell", "ref": { "item": "a" } } ] })"));
        QCOMPARE(noDir.dir, Qt::Horizontal);
        QCOMPARE(noDir.children.size(), std::size_t(2));
        QCOMPARE(noDir.children[0].kind, LayoutNode::Kind::Cell);
        QVERIFY(!noDir.children[0].ref);
        QCOMPARE(layoutToJson(noDir), parse(R"({ "type": "split", "dir": "h", "sizes": [1, 1],
            "children": [ { "type": "cell", "ref": null }, { "type": "cell", "ref": { "item": "a" } } ] })"));

        // A split without children stays empty, for normalisation to handle.
        const LayoutNode empty = layoutFromJson(parse(R"({ "type": "split", "dir": "v" })"));
        QCOMPARE(empty.kind, LayoutNode::Kind::Split);
        QVERIFY(empty.children.empty());
        QVERIFY(empty.sizes.empty());
    }

    void tabsRepairs()
    {
        // current out of range is 0. A tab without a node is an empty cell.
        const LayoutNode tabs = layoutFromJson(parse(R"({ "type": "tabs", "current": 3,
            "tabs": [ { "title": "a" }, { "node": { "type": "cell", "ref": { "item": "x" } } }, 7 ] })"));
        QCOMPARE(tabs.kind, LayoutNode::Kind::Tabs);
        QCOMPARE(tabs.current, 0);
        QVERIFY(!tabs.collapseSingle);
        QCOMPARE(tabs.children.size(), std::size_t(3));
        QCOMPARE(tabs.titles, (std::vector<QString>{QStringLiteral("a"), QString(), QString()}));
        QVERIFY(!tabs.children[0].ref);
        QCOMPARE(tabs.children[1].ref->item, QStringLiteral("x"));

        QCOMPARE(layoutFromJson(parse(R"({ "type": "tabs", "current": -1, "tabs": [ {} ] })")).current, 0);
        QCOMPARE(layoutFromJson(parse(R"({ "type": "tabs", "current": 0.5, "tabs": [ {}, {} ] })")).current, 0);
        QCOMPARE(layoutFromJson(parse(R"({ "type": "tabs", "current": 1, "tabs": [ {}, {} ] })")).current, 1);
    }

    void writesMissingSizesAndTitles()
    {
        // A tree built in code with too few sizes or titles is still written whole.
        LayoutNode split;
        split.kind = LayoutNode::Kind::Split;
        split.dir = Qt::Vertical;
        split.children.resize(2);
        QCOMPARE(layoutToJson(split), parse(R"({ "type": "split", "dir": "v", "sizes": [1, 1],
            "children": [ { "type": "cell", "ref": null }, { "type": "cell", "ref": null } ] })"));

        LayoutNode tabs;
        tabs.kind = LayoutNode::Kind::Tabs;
        tabs.children.resize(1);
        tabs.collapseSingle = true;
        QCOMPARE(layoutToJson(tabs), parse(R"({ "type": "tabs", "current": 0, "collapseSingle": true,
            "tabs": [ { "title": "", "node": { "type": "cell", "ref": null } } ] })"));
    }
};

QTEST_GUILESS_MAIN(TestLayoutJson)
#include "tst_layout_json.moc"
