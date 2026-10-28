// Tests Model: lookups, children, refExists, the layout setters and their signals.
#include "core/Model.h"

#include <QSignalSpy>
#include <QTest>

#include <cstddef>
#include <utility>

using namespace mirante;

namespace {

LayoutNode cell(const CellRef &ref)
{
    LayoutNode node;
    node.ref = ref;
    return node;
}

LayoutNode itemCell(const QString &item)
{
    return cell(CellRef{item, QString(), QString()});
}

LayoutNode hsplit(std::vector<LayoutNode> children)
{
    LayoutNode node;
    node.kind = LayoutNode::Kind::Split;
    node.dir = Qt::Horizontal;
    node.sizes.assign(children.size(), 1.0);
    node.children = std::move(children);
    return node;
}

Item makeItem(const QString &id)
{
    Item item;
    item.id = id;
    item.type = QStringLiteral("web.page");
    return item;
}

Dashboard makeDashboard(const QString &id)
{
    Dashboard dashboard;
    dashboard.id = id;
    return dashboard;
}

Task makeTask(const QString &id, const QString &projectId, std::vector<QString> items,
              std::vector<QString> dashboards)
{
    Task task;
    task.id = id;
    task.projectId = projectId;
    for (const QString &item : items)
        task.items.push_back(makeItem(item));
    for (const QString &dashboard : dashboards)
        task.dashboards.push_back(makeDashboard(dashboard));
    return task;
}

// g1 holds p1 and p2. p1 holds t1 (items i1, i2; dashboards d1, d2) and t2
// (item i3; dashboard d3). Root dashboard r1.
void fill(Model &model)
{
    Group g1;
    g1.id = QStringLiteral("g1");
    Project p1;
    p1.id = QStringLiteral("p1");
    p1.groupId = QStringLiteral("g1");
    Project p2;
    p2.id = QStringLiteral("p2");
    p2.groupId = QStringLiteral("g1");
    RootDashboard r1;
    r1.id = QStringLiteral("r1");
    r1.name = QStringLiteral("Root");

    model.reset({g1}, {p1, p2},
                {makeTask(QStringLiteral("t1"), QStringLiteral("p1"), {QStringLiteral("i1"), QStringLiteral("i2")},
                          {QStringLiteral("d1"), QStringLiteral("d2")}),
                 makeTask(QStringLiteral("t2"), QStringLiteral("p1"), {QStringLiteral("i3")},
                          {QStringLiteral("d3")})},
                {r1});
}

} // namespace

class TestModel : public QObject
{
    Q_OBJECT

private slots:
    void emptyModel();
    void resetEmitsSignal();
    void lookups();
    void children();
    void refExists_data();
    void refExists();
    void setTaskDashboardLayout();
    void taskDashboardItemsMustBelongToTask();
    void setRootDashboardLayout();
    void setLayoutDispatches();
    void setterRemovesLayoutFromExtra();
    void unknownIdsAreIgnored();
    void addRootDashboard();
    void removeRootDashboard();
    void pointersStayValid();
};

void TestModel::emptyModel()
{
    Model model;
    QVERIFY(model.groups().empty());
    QVERIFY(model.rootDashboards().empty());
    QCOMPARE(model.task(QStringLiteral("t1")), nullptr);
}

void TestModel::resetEmitsSignal()
{
    Model model;
    QSignalSpy spy(&model, &Model::modelReset);
    fill(model);
    QCOMPARE(spy.count(), 1);
}

void TestModel::lookups()
{
    Model model;
    fill(model);

    QVERIFY(model.group(QStringLiteral("g1")));
    QCOMPARE(model.group(QStringLiteral("g1"))->id, QStringLiteral("g1"));
    QCOMPARE(model.group(QStringLiteral("nope")), nullptr);
    QCOMPARE(model.project(QStringLiteral("p2"))->groupId, QStringLiteral("g1"));
    QCOMPARE(model.task(QStringLiteral("t2"))->projectId, QStringLiteral("p1"));

    QCOMPARE(model.item(QStringLiteral("t1"), QStringLiteral("i2"))->id, QStringLiteral("i2"));
    QCOMPARE(model.item(QStringLiteral("t2"), QStringLiteral("i2")), nullptr);
    QCOMPARE(model.item(QStringLiteral("nope"), QStringLiteral("i2")), nullptr);

    QCOMPARE(model.rootDashboard(QStringLiteral("r1"))->name, QStringLiteral("Root"));
    QCOMPARE(model.rootDashboard(QStringLiteral("d1")), nullptr);
    QCOMPARE(model.taskDashboard(QStringLiteral("t1"), QStringLiteral("d2"))->id, QStringLiteral("d2"));
    QCOMPARE(model.taskDashboard(QStringLiteral("t2"), QStringLiteral("d2")), nullptr);

    QCOMPARE(model.layout(DashboardRef{QString(), QStringLiteral("r1")}),
             &model.rootDashboard(QStringLiteral("r1"))->layout);
    QCOMPARE(model.layout(DashboardRef{QStringLiteral("t1"), QStringLiteral("d1")}),
             &model.taskDashboard(QStringLiteral("t1"), QStringLiteral("d1"))->layout);
    QCOMPARE(model.layout(DashboardRef{QStringLiteral("t1"), QStringLiteral("d3")}), nullptr);

    QCOMPARE(model.taskOfItem(QStringLiteral("i3")), model.task(QStringLiteral("t2")));
    QCOMPARE(model.taskOfItem(QStringLiteral("nope")), nullptr);
}

void TestModel::children()
{
    Model model;
    fill(model);

    QCOMPARE(model.groups().size(), std::size_t(1));
    QCOMPARE(model.projectsOfGroup(QStringLiteral("g1")).size(), std::size_t(2));
    QVERIFY(model.projectsOfGroup(QStringLiteral("nope")).empty());
    QCOMPARE(model.tasksOfProject(QStringLiteral("p1")).size(), std::size_t(2));
    QVERIFY(model.tasksOfProject(QStringLiteral("p2")).empty());
    QCOMPARE(model.rootDashboards().size(), std::size_t(1));
}

void TestModel::refExists_data()
{
    QTest::addColumn<QString>("item");
    QTest::addColumn<QString>("task");
    QTest::addColumn<QString>("dashboard");
    QTest::addColumn<bool>("exists");

    QTest::newRow("item") << "i1" << "" << "" << true;
    QTest::newRow("unknown item") << "x" << "" << "" << false;
    QTest::newRow("item of task") << "i3" << "t2" << "" << true;
    QTest::newRow("item of other task") << "i1" << "t2" << "" << false;
    QTest::newRow("item of unknown task") << "i1" << "x" << "" << false;
    QTest::newRow("task dashboard") << "" << "t1" << "d2" << true;
    QTest::newRow("dashboard of other task") << "" << "t2" << "d2" << false;
    QTest::newRow("unknown dashboard") << "" << "t1" << "x" << false;
    QTest::newRow("task") << "" << "t2" << "" << true;
    QTest::newRow("unknown task") << "" << "x" << "" << false;
    QTest::newRow("nothing") << "" << "" << "" << false;
}

void TestModel::refExists()
{
    QFETCH(QString, item);
    QFETCH(QString, task);
    QFETCH(QString, dashboard);
    QFETCH(bool, exists);

    Model model;
    fill(model);
    QCOMPARE(model.refExists(CellRef{item, task, dashboard}), exists);
}

void TestModel::setTaskDashboardLayout()
{
    Model model;
    fill(model);
    QSignalSpy spy(&model, &Model::taskChanged);

    // Normalisation replaces the single-child split with its child.
    model.setTaskDashboardLayout(QStringLiteral("t1"), QStringLiteral("d1"), hsplit({itemCell(QStringLiteral("i1"))}));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("t1"));
    const LayoutNode *layout = model.layout(DashboardRef{QStringLiteral("t1"), QStringLiteral("d1")});
    QCOMPARE(layout->kind, LayoutNode::Kind::Cell);
    QVERIFY(layout->ref);
    QCOMPARE(layout->ref->item, QStringLiteral("i1"));
    QVERIFY(!layout->removed);
}

void TestModel::taskDashboardItemsMustBelongToTask()
{
    Model model;
    fill(model);

    // i3 is an item of t2, so in a dashboard of t1 it is removed.
    model.setTaskDashboardLayout(QStringLiteral("t1"), QStringLiteral("d1"),
                                 hsplit({itemCell(QStringLiteral("i2")), itemCell(QStringLiteral("i3"))}));

    const LayoutNode *layout = model.layout(DashboardRef{QStringLiteral("t1"), QStringLiteral("d1")});
    QCOMPARE(layout->kind, LayoutNode::Kind::Split);
    QCOMPARE(layout->children.size(), std::size_t(2));
    QVERIFY(!layout->children[0].removed);
    QVERIFY(layout->children[1].removed);
    QVERIFY(!layout->children[1].ref);
}

void TestModel::setRootDashboardLayout()
{
    Model model;
    fill(model);
    QSignalSpy spy(&model, &Model::rootDashboardChanged);

    model.setRootDashboardLayout(QStringLiteral("r1"),
                                 hsplit({cell(CellRef{QString(), QStringLiteral("t1"), QStringLiteral("d2")}),
                                         cell(CellRef{QStringLiteral("i3"), QStringLiteral("t2"), QString()}),
                                         cell(CellRef{QString(), QStringLiteral("gone"), QString()})}));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("r1"));
    const LayoutNode *layout = model.layout(DashboardRef{QString(), QStringLiteral("r1")});
    QCOMPARE(layout->children.size(), std::size_t(3));
    QVERIFY(!layout->children[0].removed);
    QVERIFY(!layout->children[1].removed);
    QVERIFY(layout->children[2].removed);
}

void TestModel::setLayoutDispatches()
{
    Model model;
    fill(model);
    QSignalSpy taskSpy(&model, &Model::taskChanged);
    QSignalSpy rootSpy(&model, &Model::rootDashboardChanged);

    model.setLayout(DashboardRef{QStringLiteral("t2"), QStringLiteral("d3")}, itemCell(QStringLiteral("i3")));
    model.setLayout(DashboardRef{QString(), QStringLiteral("r1")}, itemCell(QStringLiteral("i3")));

    QCOMPARE(taskSpy.count(), 1);
    QCOMPARE(rootSpy.count(), 1);
    QCOMPARE(model.layout(DashboardRef{QStringLiteral("t2"), QStringLiteral("d3")})->ref->item,
             QStringLiteral("i3"));
    QCOMPARE(model.layout(DashboardRef{QString(), QStringLiteral("r1")})->ref->item, QStringLiteral("i3"));
}

void TestModel::setterRemovesLayoutFromExtra()
{
    Model model;
    Group group;
    group.id = QStringLiteral("g");
    Project project;
    project.id = QStringLiteral("p");
    project.groupId = QStringLiteral("g");
    Task task = makeTask(QStringLiteral("t"), QStringLiteral("p"), {QStringLiteral("i")}, {QStringLiteral("d")});
    task.dashboards[0].extra.insert(QStringLiteral("layout"), QStringLiteral("unusable"));
    task.dashboards[0].extra.insert(QStringLiteral("other"), 1);
    RootDashboard root;
    root.id = QStringLiteral("r");
    root.extra.insert(QStringLiteral("layout"), 42);
    model.reset({group}, {project}, {task}, {root});

    model.setTaskDashboardLayout(QStringLiteral("t"), QStringLiteral("d"), itemCell(QStringLiteral("i")));
    model.setRootDashboardLayout(QStringLiteral("r"), itemCell(QStringLiteral("i")));

    const Dashboard *dashboard = model.taskDashboard(QStringLiteral("t"), QStringLiteral("d"));
    QVERIFY(!dashboard->extra.contains(QStringLiteral("layout")));
    QVERIFY(dashboard->extra.contains(QStringLiteral("other")));
    QCOMPARE(dashboard->toJson().value(QStringLiteral("layout")).toObject().value(QStringLiteral("type")).toString(),
             QStringLiteral("cell"));
    QVERIFY(!model.rootDashboard(QStringLiteral("r"))->extra.contains(QStringLiteral("layout")));
}

void TestModel::unknownIdsAreIgnored()
{
    Model model;
    fill(model);
    QSignalSpy taskSpy(&model, &Model::taskChanged);
    QSignalSpy rootSpy(&model, &Model::rootDashboardChanged);

    model.setTaskDashboardLayout(QStringLiteral("x"), QStringLiteral("d1"), itemCell(QStringLiteral("i1")));
    model.setTaskDashboardLayout(QStringLiteral("t1"), QStringLiteral("x"), itemCell(QStringLiteral("i1")));
    model.setRootDashboardLayout(QStringLiteral("x"), itemCell(QStringLiteral("i1")));

    QCOMPARE(taskSpy.count(), 0);
    QCOMPARE(rootSpy.count(), 0);
}

void TestModel::addRootDashboard()
{
    Model model;
    fill(model);
    QSignalSpy spy(&model, &Model::rootDashboardAdded);

    RootDashboard added;
    added.id = QStringLiteral("r2");
    added.name = QStringLiteral("New");
    added.order = 3;
    added.layout = hsplit({itemCell(QStringLiteral("gone"))});
    model.addRootDashboard(added);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("r2"));
    QCOMPARE(model.rootDashboards().size(), std::size_t(2));
    const RootDashboard *stored = model.rootDashboard(QStringLiteral("r2"));
    QCOMPARE(stored->name, QStringLiteral("New"));
    QCOMPARE(stored->order, 3);
    // Normalised: the single-child split is gone and the dangling cell is removed.
    QCOMPARE(stored->layout.kind, LayoutNode::Kind::Cell);
    QVERIFY(stored->layout.removed);

    // An existing id is ignored.
    model.addRootDashboard(added);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.rootDashboards().size(), std::size_t(2));
}

void TestModel::removeRootDashboard()
{
    Model model;
    fill(model);
    const std::size_t before = model.rootDashboards().size();
    const QString id = model.rootDashboards().front()->id;
    QSignalSpy spy(&model, &Model::rootDashboardRemoved);

    model.removeRootDashboard(id);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), id);
    QVERIFY(!model.rootDashboard(id));
    QCOMPARE(model.rootDashboards().size(), before - 1);

    // An unknown id changes nothing.
    model.removeRootDashboard(QStringLiteral("no-such-id"));
    QCOMPARE(spy.count(), 1);
}

void TestModel::pointersStayValid()
{
    Model model;
    fill(model);
    const Task *task = model.task(QStringLiteral("t1"));
    const Item *item = model.item(QStringLiteral("t1"), QStringLiteral("i1"));
    const Dashboard *dashboard = model.taskDashboard(QStringLiteral("t1"), QStringLiteral("d1"));
    const RootDashboard *root = model.rootDashboard(QStringLiteral("r1"));

    for (int i = 0; i < 20; ++i) {
        RootDashboard added;
        added.id = QStringLiteral("extra%1").arg(i);
        model.addRootDashboard(added);
    }
    model.setTaskDashboardLayout(QStringLiteral("t1"), QStringLiteral("d1"), itemCell(QStringLiteral("i2")));
    model.setRootDashboardLayout(QStringLiteral("r1"), itemCell(QStringLiteral("i2")));

    QCOMPARE(model.task(QStringLiteral("t1")), task);
    QCOMPARE(model.item(QStringLiteral("t1"), QStringLiteral("i1")), item);
    QCOMPARE(model.taskDashboard(QStringLiteral("t1"), QStringLiteral("d1")), dashboard);
    QCOMPARE(model.rootDashboard(QStringLiteral("r1")), root);
    QCOMPARE(dashboard->layout.ref->item, QStringLiteral("i2"));
    QCOMPARE(root->layout.ref->item, QStringLiteral("i2"));
}

QTEST_GUILESS_MAIN(TestModel)
#include "tst_model.moc"
