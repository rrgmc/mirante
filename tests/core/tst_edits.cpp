// Tests core/Edits: the pure functions that change tasks and reorder siblings.
#include "core/Edits.h"

#include <QJsonArray>
#include <QTest>
#include <QUuid>

#include <algorithm>
#include <utility>

using namespace mirante;

namespace {

LayoutNode itemCell(const QString &item)
{
    LayoutNode node;
    node.ref = CellRef{item, QString(), QString()};
    return node;
}

LayoutNode hsplit(std::vector<LayoutNode> children)
{
    LayoutNode node;
    node.kind = LayoutNode::Kind::Split;
    node.sizes.assign(children.size(), 1.0);
    node.children = std::move(children);
    return node;
}

Item makeItem(const QString &id, const QJsonObject &config = {})
{
    Item item;
    item.id = id;
    item.type = QStringLiteral("web.page");
    item.config = config;
    return item;
}

Dashboard makeDashboard(const QString &id, const LayoutNode &layout = LayoutNode())
{
    Dashboard dashboard;
    dashboard.id = id;
    dashboard.layout = layout;
    return dashboard;
}

// Items i1, i2. Dashboard d1 shows i1 and i2 side by side; d2 shows i1. d1 is the default.
Task sampleTask()
{
    Task task;
    task.id = QStringLiteral("t1");
    task.projectId = QStringLiteral("p1");
    task.items = {makeItem(QStringLiteral("i1")), makeItem(QStringLiteral("i2"))};
    task.dashboards = {makeDashboard(QStringLiteral("d1"),
                                     hsplit({itemCell(QStringLiteral("i1")), itemCell(QStringLiteral("i2"))})),
                       makeDashboard(QStringLiteral("d2"), itemCell(QStringLiteral("i1")))};
    task.defaultDashboardId = QStringLiteral("d1");
    return task;
}

Project makeProject(const QString &id, Priority priority, int order)
{
    Project project;
    project.id = id;
    project.groupId = QStringLiteral("g1");
    project.priority = priority;
    project.order = order;
    return project;
}

Task makeTask(const QString &id, Priority priority, int order)
{
    Task task;
    task.id = id;
    task.projectId = QStringLiteral("p1");
    task.priority = priority;
    task.order = order;
    return task;
}

Group makeGroup(const QString &id, int order)
{
    Group group;
    group.id = id;
    group.order = order;
    return group;
}

template <typename T>
const T *byId(const std::vector<T> &list, const QString &id)
{
    const auto it = std::find_if(list.begin(), list.end(), [&id](const T &e) { return e.id == id; });
    return it == list.end() ? nullptr : &*it;
}

template <typename T>
QStringList ids(const std::vector<T> &list)
{
    QStringList result;
    for (const T &e : list)
        result.append(e.id);
    result.sort();
    return result;
}

} // namespace

class TestEdits : public QObject
{
    Q_OBJECT

private slots:
    void newId();
    void newTask();
    void addItem();
    void addItemReplacesSameId();
    void removeItem();
    void removeUnknownItem();
    void addDashboard();
    void addFirstDashboardBecomesDefault();
    void removeDashboard();
    void removeDefaultDashboard();
    void removeLastDashboardRefused();
    void setDefaultDashboard();
    void moveProjectWithinPriority();
    void moveProjectToOtherPriority();
    void moveProjectClampsIndex();
    void moveUnknownProject();
    void moveTask();
    void moveGroup();
    void referencedStrings();
};

void TestEdits::newId()
{
    const QString id = edits::newId();
    QVERIFY(!id.startsWith(QLatin1Char('{')));
    QVERIFY(!QUuid::fromString(id).isNull());
    QVERIFY(edits::newId() != id);
}

void TestEdits::newTask()
{
    const Task task = edits::newTask(QStringLiteral("p1"), QStringLiteral("Fix login"));
    QVERIFY(!QUuid::fromString(task.id).isNull());
    QCOMPARE(task.projectId, QStringLiteral("p1"));
    QCOMPARE(task.title, QStringLiteral("Fix login"));
    QCOMPARE(task.state, TaskState::Active);
    QVERIFY(task.items.empty());
    QCOMPARE(task.dashboards.size(), std::size_t(1));
    QCOMPARE(task.dashboards[0].name, QStringLiteral("Main"));
    QCOMPARE(task.defaultDashboardId, task.dashboards[0].id);
    QVERIFY(task.dashboards[0].id != task.id);
    const LayoutNode &layout = task.dashboards[0].layout;
    QCOMPARE(layout.kind, LayoutNode::Kind::Cell);
    QVERIFY(!layout.ref);
}

void TestEdits::addItem()
{
    const Task task = edits::addItem(sampleTask(), makeItem(QStringLiteral("i3")));
    QCOMPARE(task.items.size(), std::size_t(3));
    QCOMPARE(task.items[2].id, QStringLiteral("i3"));
}

void TestEdits::addItemReplacesSameId()
{
    Item renamed = makeItem(QStringLiteral("i1"));
    renamed.name = QStringLiteral("Renamed");
    const Task task = edits::addItem(sampleTask(), renamed);
    QCOMPARE(task.items.size(), std::size_t(2));
    QCOMPARE(task.items[0].name, QStringLiteral("Renamed"));
}

void TestEdits::removeItem()
{
    const Task task = edits::removeItem(sampleTask(), QStringLiteral("i1"));
    QCOMPARE(task.items.size(), std::size_t(1));
    QCOMPARE(task.items[0].id, QStringLiteral("i2"));

    // The cells that showed i1 are empty; the split stays.
    const LayoutNode &d1 = task.dashboards[0].layout;
    QCOMPARE(d1.kind, LayoutNode::Kind::Split);
    QVERIFY(!d1.children[0].ref);
    QCOMPARE(d1.children[1].ref->item, QStringLiteral("i2"));
    QVERIFY(!task.dashboards[1].layout.ref);
}

void TestEdits::removeUnknownItem()
{
    QCOMPARE(edits::removeItem(sampleTask(), QStringLiteral("nope")), sampleTask());
}

void TestEdits::addDashboard()
{
    const Task task = edits::addDashboard(sampleTask(), makeDashboard(QStringLiteral("d3")));
    QCOMPARE(task.dashboards.size(), std::size_t(3));
    QCOMPARE(task.dashboards[2].id, QStringLiteral("d3"));
    QCOMPARE(task.defaultDashboardId, QStringLiteral("d1"));
}

void TestEdits::addFirstDashboardBecomesDefault()
{
    Task empty;
    empty.id = QStringLiteral("t1");
    const Task task = edits::addDashboard(empty, makeDashboard(QStringLiteral("d1")));
    QCOMPARE(task.defaultDashboardId, QStringLiteral("d1"));
}

void TestEdits::removeDashboard()
{
    const Task task = edits::removeDashboard(sampleTask(), QStringLiteral("d2"));
    QCOMPARE(task.dashboards.size(), std::size_t(1));
    QCOMPARE(task.dashboards[0].id, QStringLiteral("d1"));
    QCOMPARE(task.defaultDashboardId, QStringLiteral("d1"));
}

void TestEdits::removeDefaultDashboard()
{
    const Task task = edits::removeDashboard(sampleTask(), QStringLiteral("d1"));
    QCOMPARE(task.dashboards.size(), std::size_t(1));
    QCOMPARE(task.defaultDashboardId, QStringLiteral("d2"));
}

void TestEdits::removeLastDashboardRefused()
{
    const Task one = edits::removeDashboard(sampleTask(), QStringLiteral("d2"));
    QCOMPARE(edits::removeDashboard(one, QStringLiteral("d1")), one);
    QCOMPARE(edits::removeDashboard(sampleTask(), QStringLiteral("nope")), sampleTask());
}

void TestEdits::setDefaultDashboard()
{
    QCOMPARE(edits::setDefaultDashboard(sampleTask(), QStringLiteral("d2")).defaultDashboardId,
             QStringLiteral("d2"));
    QCOMPARE(edits::setDefaultDashboard(sampleTask(), QStringLiteral("nope")).defaultDashboardId,
             QStringLiteral("d1"));
}

void TestEdits::moveProjectWithinPriority()
{
    // Normal: a (0), b (1), c (2). High: h (0).
    const std::vector<Project> siblings{
        makeProject(QStringLiteral("a"), Priority::Normal, 0), makeProject(QStringLiteral("b"), Priority::Normal, 1),
        makeProject(QStringLiteral("c"), Priority::Normal, 2), makeProject(QStringLiteral("h"), Priority::High, 0)};

    // c to the front: c 0, a 1, b 2.
    const std::vector<Project> changed = edits::moveProject(siblings, QStringLiteral("c"), Priority::Normal, 0);
    QCOMPARE(ids(changed), QStringList({QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")}));
    QCOMPARE(byId(changed, QStringLiteral("c"))->order, 0);
    QCOMPARE(byId(changed, QStringLiteral("a"))->order, 1);
    QCOMPARE(byId(changed, QStringLiteral("b"))->order, 2);

    // a after b: b 0, a 1; c keeps 2 and is not returned.
    const std::vector<Project> swapped = edits::moveProject(siblings, QStringLiteral("a"), Priority::Normal, 1);
    QCOMPARE(ids(swapped), QStringList({QStringLiteral("a"), QStringLiteral("b")}));
    QCOMPARE(byId(swapped, QStringLiteral("b"))->order, 0);
    QCOMPARE(byId(swapped, QStringLiteral("a"))->order, 1);

    // A move to the same place changes nothing.
    QVERIFY(edits::moveProject(siblings, QStringLiteral("b"), Priority::Normal, 1).empty());
}

void TestEdits::moveProjectToOtherPriority()
{
    // Normal: a (0), b (1). High: h (0), k (1).
    const std::vector<Project> siblings{
        makeProject(QStringLiteral("a"), Priority::Normal, 0), makeProject(QStringLiteral("b"), Priority::Normal, 1),
        makeProject(QStringLiteral("h"), Priority::High, 0), makeProject(QStringLiteral("k"), Priority::High, 1)};

    // a between h and k. b keeps its order, so it is not returned.
    const std::vector<Project> changed = edits::moveProject(siblings, QStringLiteral("a"), Priority::High, 1);
    QCOMPARE(ids(changed), QStringList({QStringLiteral("a"), QStringLiteral("k")}));
    QCOMPARE(byId(changed, QStringLiteral("a"))->priority, Priority::High);
    QCOMPARE(byId(changed, QStringLiteral("a"))->order, 1);
    QCOMPARE(byId(changed, QStringLiteral("k"))->order, 2);
}

void TestEdits::moveProjectClampsIndex()
{
    const std::vector<Project> siblings{makeProject(QStringLiteral("a"), Priority::Normal, 0),
                                        makeProject(QStringLiteral("b"), Priority::Normal, 1)};
    const std::vector<Project> changed = edits::moveProject(siblings, QStringLiteral("a"), Priority::Normal, 99);
    QCOMPARE(byId(changed, QStringLiteral("a"))->order, 1);
    QCOMPARE(byId(changed, QStringLiteral("b"))->order, 0);

    const std::vector<Project> front = edits::moveProject(siblings, QStringLiteral("b"), Priority::Normal, -5);
    QCOMPARE(byId(front, QStringLiteral("b"))->order, 0);
}

void TestEdits::moveUnknownProject()
{
    const std::vector<Project> siblings{makeProject(QStringLiteral("a"), Priority::Normal, 0)};
    QVERIFY(edits::moveProject(siblings, QStringLiteral("nope"), Priority::Normal, 0).empty());
}

void TestEdits::moveTask()
{
    // Equal orders are sorted by title, then id, before renumbering.
    std::vector<Task> siblings{makeTask(QStringLiteral("a"), Priority::Low, 5),
                               makeTask(QStringLiteral("b"), Priority::Low, 5),
                               makeTask(QStringLiteral("u"), Priority::Urgent, 0)};
    const std::vector<Task> changed = edits::moveTask(siblings, QStringLiteral("u"), Priority::Low, 1);
    QCOMPARE(ids(changed), QStringList({QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("u")}));
    QCOMPARE(byId(changed, QStringLiteral("a"))->order, 0);
    QCOMPARE(byId(changed, QStringLiteral("u"))->order, 1);
    QCOMPARE(byId(changed, QStringLiteral("u"))->priority, Priority::Low);
    QCOMPARE(byId(changed, QStringLiteral("b"))->order, 2);
}

void TestEdits::moveGroup()
{
    const std::vector<Group> groups{makeGroup(QStringLiteral("a"), 0), makeGroup(QStringLiteral("b"), 1),
                                    makeGroup(QStringLiteral("c"), 2)};
    const std::vector<Group> changed = edits::moveGroup(groups, QStringLiteral("a"), 2);
    QCOMPARE(ids(changed), QStringList({QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")}));
    QCOMPARE(byId(changed, QStringLiteral("b"))->order, 0);
    QCOMPARE(byId(changed, QStringLiteral("c"))->order, 1);
    QCOMPARE(byId(changed, QStringLiteral("a"))->order, 2);
    QVERIFY(edits::moveGroup(groups, QStringLiteral("c"), 2).empty());
}

void TestEdits::referencedStrings()
{
    Task t1 = sampleTask();
    t1.items[0].config = QJsonObject{{QStringLiteral("file"), QStringLiteral("a.png")},
                                     {QStringLiteral("count"), 3},
                                     {QStringLiteral("flag"), true}};
    t1.items[1].config = QJsonObject{
        {QStringLiteral("nested"),
         QJsonObject{{QStringLiteral("list"), QJsonArray{QStringLiteral("b.png"), QJsonObject{{QStringLiteral("x"), QStringLiteral("c")}}}}}}};
    Task t2 = sampleTask();
    t2.items = {makeItem(QStringLiteral("i9"), QJsonObject{{QStringLiteral("url"), QStringLiteral("https://x")}})};
    // Item ids, names and types are not config values.
    t2.items[0].name = QStringLiteral("name.png");

    const QSet<QString> strings = edits::referencedStrings({&t1, &t2, nullptr});
    QCOMPARE(strings, QSet<QString>({QStringLiteral("a.png"), QStringLiteral("b.png"), QStringLiteral("c"),
                                     QStringLiteral("https://x")}));
    QVERIFY(edits::referencedStrings({}).isEmpty());
}

QTEST_GUILESS_MAIN(TestEdits)
#include "tst_edits.moc"
