// Tests the M3 Model setters and removers: adding, replacing, moving, the
// cascades, the item index, normalisation and the signals emitted.
#include "core/Edits.h"
#include "core/Model.h"

#include <QSignalSpy>
#include <QTest>

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
    node.sizes.assign(children.size(), 1.0);
    node.children = std::move(children);
    return node;
}

Group makeGroup(const QString &id)
{
    Group group;
    group.id = id;
    group.name = id;
    return group;
}

Project makeProject(const QString &id, const QString &groupId)
{
    Project project;
    project.id = id;
    project.groupId = groupId;
    return project;
}

Item makeItem(const QString &id)
{
    Item item;
    item.id = id;
    item.type = QStringLiteral("web.page");
    return item;
}

// A task with the given items and one dashboard "<id>-d" showing one empty cell.
Task makeTask(const QString &id, const QString &projectId, const std::vector<QString> &items = {})
{
    Task task;
    task.id = id;
    task.projectId = projectId;
    for (const QString &item : items)
        task.items.push_back(makeItem(item));
    Dashboard dashboard;
    dashboard.id = id + QStringLiteral("-d");
    task.dashboards.push_back(dashboard);
    task.defaultDashboardId = dashboard.id;
    return task;
}

// g1 holds p1 and p2; g2 holds p3. p1 holds t1 (i1) and t2 (i2); p2 holds t3; p3 holds t4.
void fill(Model &model)
{
    model.reset({makeGroup(QStringLiteral("g1")), makeGroup(QStringLiteral("g2"))},
                {makeProject(QStringLiteral("p1"), QStringLiteral("g1")),
                 makeProject(QStringLiteral("p2"), QStringLiteral("g1")),
                 makeProject(QStringLiteral("p3"), QStringLiteral("g2"))},
                {makeTask(QStringLiteral("t1"), QStringLiteral("p1"), {QStringLiteral("i1")}),
                 makeTask(QStringLiteral("t2"), QStringLiteral("p1"), {QStringLiteral("i2")}),
                 makeTask(QStringLiteral("t3"), QStringLiteral("p2")),
                 makeTask(QStringLiteral("t4"), QStringLiteral("p3"))},
                {});
}

// Records every add, change and remove signal of a model, in order, as
// "<signal> <id>".
class Recorder : public QObject
{
public:
    explicit Recorder(Model &model)
    {
        const auto on = [this, &model](auto signal, const char *name) {
            connect(&model, signal, this, [this, name](const QString &id) {
                events.append(QStringLiteral("%1 %2").arg(QLatin1String(name), id));
            });
        };
        on(&Model::groupAdded, "groupAdded");
        on(&Model::groupChanged, "groupChanged");
        on(&Model::groupRemoved, "groupRemoved");
        on(&Model::projectAdded, "projectAdded");
        on(&Model::projectChanged, "projectChanged");
        on(&Model::projectRemoved, "projectRemoved");
        on(&Model::taskAdded, "taskAdded");
        on(&Model::taskChanged, "taskChanged");
        on(&Model::taskRemoved, "taskRemoved");
        on(&Model::rootDashboardAdded, "rootDashboardAdded");
        on(&Model::rootDashboardChanged, "rootDashboardChanged");
        on(&Model::rootDashboardRemoved, "rootDashboardRemoved");
    }

    QStringList events;
};

} // namespace

class TestModelEdits : public QObject
{
    Q_OBJECT

private slots:
    void setGroupAddsAndChanges();
    void setProjectMoves();
    void setTaskAddsAndChanges();
    void setTaskMoves();
    void equalValuesEmitNothing();
    void emptyIdsAreIgnored();
    void setTaskNormalises();
    void setTaskUpdatesItemIndex();
    void setTaskDropsStaleExtraLayout();
    void setRootDashboard();
    void removeTask();
    void removeProjectCascades();
    void removeGroupCascades();
    void removeUnknownEmitsNothing();
    void removeTaskKeepsRootDashboards();
    void removeWithOwnId();
};

void TestModelEdits::setGroupAddsAndChanges()
{
    Model model;
    fill(model);
    Recorder recorder(model);

    model.setGroup(makeGroup(QStringLiteral("g3")));
    QVERIFY(model.group(QStringLiteral("g3")));

    Group renamed = *model.group(QStringLiteral("g1"));
    renamed.name = QStringLiteral("Work");
    model.setGroup(renamed);
    QCOMPARE(model.group(QStringLiteral("g1"))->name, QStringLiteral("Work"));

    QCOMPARE(recorder.events, QStringList({QStringLiteral("groupAdded g3"), QStringLiteral("groupChanged g1")}));
}

void TestModelEdits::setProjectMoves()
{
    Model model;
    fill(model);
    Recorder recorder(model);

    Project moved = *model.project(QStringLiteral("p2"));
    moved.groupId = QStringLiteral("g2");
    model.setProject(moved);

    QCOMPARE(model.projectsOfGroup(QStringLiteral("g1")).size(), std::size_t(1));
    QCOMPARE(model.projectsOfGroup(QStringLiteral("g2")).size(), std::size_t(2));
    QCOMPARE(recorder.events, QStringList({QStringLiteral("projectChanged p2")}));

    model.setProject(makeProject(QStringLiteral("p9"), QStringLiteral("g1")));
    QCOMPARE(recorder.events.last(), QStringLiteral("projectAdded p9"));
}

void TestModelEdits::setTaskAddsAndChanges()
{
    Model model;
    fill(model);
    Recorder recorder(model);

    const Task created = edits::newTask(QStringLiteral("p1"), QStringLiteral("New"));
    model.setTask(created);
    QVERIFY(model.task(created.id));
    QCOMPARE(model.task(created.id)->title, QStringLiteral("New"));

    Task changed = *model.task(QStringLiteral("t1"));
    changed.state = TaskState::Done;
    model.setTask(changed);
    QCOMPARE(model.task(QStringLiteral("t1"))->state, TaskState::Done);

    QCOMPARE(recorder.events,
             QStringList({QStringLiteral("taskAdded ") + created.id, QStringLiteral("taskChanged t1")}));
}

void TestModelEdits::setTaskMoves()
{
    Model model;
    fill(model);

    Task moved = *model.task(QStringLiteral("t1"));
    moved.projectId = QStringLiteral("p3");
    model.setTask(moved);
    QCOMPARE(model.tasksOfProject(QStringLiteral("p1")).size(), std::size_t(1));
    QCOMPARE(model.tasksOfProject(QStringLiteral("p3")).size(), std::size_t(2));
}

void TestModelEdits::equalValuesEmitNothing()
{
    Model model;
    fill(model);
    RootDashboard root;
    root.id = QStringLiteral("r1");
    root.layout = cell(CellRef{QString(), QStringLiteral("t1"), QString()});
    model.setRootDashboard(root);
    Recorder recorder(model);

    model.setGroup(*model.group(QStringLiteral("g1")));
    model.setProject(*model.project(QStringLiteral("p1")));
    model.setTask(*model.task(QStringLiteral("t1")));
    model.setRootDashboard(*model.rootDashboard(QStringLiteral("r1")));
    model.setRootDashboard(root);
    QVERIFY(recorder.events.isEmpty());

    // extra counts: a change there alone is a change.
    Group withExtra = *model.group(QStringLiteral("g1"));
    withExtra.extra.insert(QStringLiteral("future"), 1);
    model.setGroup(withExtra);
    QCOMPARE(recorder.events, QStringList({QStringLiteral("groupChanged g1")}));
}

void TestModelEdits::emptyIdsAreIgnored()
{
    Model model;
    Recorder recorder(model);
    model.setGroup(Group());
    model.setProject(Project());
    model.setTask(Task());
    model.setRootDashboard(RootDashboard());
    QVERIFY(recorder.events.isEmpty());
    QVERIFY(model.groups().empty());
}

void TestModelEdits::setTaskNormalises()
{
    Model model;
    fill(model);

    // A split with one child collapses, and a cell showing an item of
    // another task is marked removed.
    Task task = *model.task(QStringLiteral("t1"));
    task.dashboards[0].layout = hsplit({hsplit({itemCell(QStringLiteral("i1")), itemCell(QStringLiteral("i2"))})});
    model.setTask(task);

    const LayoutNode &layout = model.task(QStringLiteral("t1"))->dashboards[0].layout;
    QCOMPARE(layout.kind, LayoutNode::Kind::Split);
    QCOMPARE(layout.children.size(), std::size_t(2));
    QVERIFY(!layout.children[0].removed);
    QVERIFY(layout.children[1].removed);

    // The new task's items count, not the stored ones: an item added in the
    // same edit is shown.
    Task withItem = edits::addItem(*model.task(QStringLiteral("t1")), makeItem(QStringLiteral("i5")));
    withItem.dashboards[0].layout = itemCell(QStringLiteral("i5"));
    model.setTask(withItem);
    QVERIFY(!model.task(QStringLiteral("t1"))->dashboards[0].layout.removed);
}

void TestModelEdits::setTaskUpdatesItemIndex()
{
    Model model;
    fill(model);

    Task task = edits::addItem(*model.task(QStringLiteral("t1")), makeItem(QStringLiteral("i5")));
    model.setTask(task);
    QCOMPARE(model.taskOfItem(QStringLiteral("i5"))->id, QStringLiteral("t1"));

    model.setTask(edits::removeItem(*model.task(QStringLiteral("t1")), QStringLiteral("i1")));
    QCOMPARE(model.taskOfItem(QStringLiteral("i1")), nullptr);
    QCOMPARE(model.taskOfItem(QStringLiteral("i5"))->id, QStringLiteral("t1"));

    const Task added = makeTask(QStringLiteral("t9"), QStringLiteral("p1"), {QStringLiteral("i9")});
    model.setTask(added);
    QCOMPARE(model.taskOfItem(QStringLiteral("i9"))->id, QStringLiteral("t9"));
    QVERIFY(model.refExists(CellRef{QStringLiteral("i9"), QString(), QString()}));

    model.removeTask(QStringLiteral("t9"));
    QCOMPARE(model.taskOfItem(QStringLiteral("i9")), nullptr);
    QVERIFY(!model.refExists(CellRef{QStringLiteral("i9"), QString(), QString()}));
}

void TestModelEdits::setTaskDropsStaleExtraLayout()
{
    // A dashboard whose layout could not be read keeps it in extra.
    Task task = makeTask(QStringLiteral("t1"), QStringLiteral("p1"), {QStringLiteral("i1")});
    task.dashboards[0].extra.insert(QStringLiteral("layout"), QStringLiteral("unreadable"));
    Model model;
    model.reset({}, {}, {task}, {});

    // Setting it unchanged keeps it, so the file is written back as it was.
    model.setTask(task);
    QVERIFY(model.task(QStringLiteral("t1"))->dashboards[0].extra.contains(QStringLiteral("layout")));

    // An edit of the layout drops it, or it would overwrite the edit.
    Task edited = *model.task(QStringLiteral("t1"));
    edited.dashboards[0].layout = itemCell(QStringLiteral("i1"));
    model.setTask(edited);
    const Dashboard &dashboard = model.task(QStringLiteral("t1"))->dashboards[0];
    QVERIFY(!dashboard.extra.contains(QStringLiteral("layout")));
    QCOMPARE(dashboard.layout.ref->item, QStringLiteral("i1"));
}

void TestModelEdits::setRootDashboard()
{
    Model model;
    fill(model);
    Recorder recorder(model);

    RootDashboard root;
    root.id = QStringLiteral("r1");
    root.layout = hsplit({cell(CellRef{QString(), QStringLiteral("t1"), QString()}),
                          cell(CellRef{QString(), QStringLiteral("gone"), QString()})});
    model.setRootDashboard(root);
    const RootDashboard *stored = model.rootDashboard(QStringLiteral("r1"));
    QVERIFY(stored);
    QVERIFY(!stored->layout.children[0].removed);
    QVERIFY(stored->layout.children[1].removed);

    root.name = QStringLiteral("Renamed");
    model.setRootDashboard(root);
    QCOMPARE(model.rootDashboard(QStringLiteral("r1"))->name, QStringLiteral("Renamed"));

    QCOMPARE(recorder.events,
             QStringList({QStringLiteral("rootDashboardAdded r1"), QStringLiteral("rootDashboardChanged r1")}));
}

void TestModelEdits::removeTask()
{
    Model model;
    fill(model);
    Recorder recorder(model);

    model.removeTask(QStringLiteral("t1"));
    QCOMPARE(model.task(QStringLiteral("t1")), nullptr);
    QCOMPARE(model.taskOfItem(QStringLiteral("i1")), nullptr);
    QCOMPARE(model.tasksOfProject(QStringLiteral("p1")).size(), std::size_t(1));
    QCOMPARE(recorder.events, QStringList({QStringLiteral("taskRemoved t1")}));
}

void TestModelEdits::removeProjectCascades()
{
    Model model;
    fill(model);
    Recorder recorder(model);

    model.removeProject(QStringLiteral("p1"));
    QCOMPARE(model.project(QStringLiteral("p1")), nullptr);
    QCOMPARE(model.task(QStringLiteral("t1")), nullptr);
    QCOMPARE(model.task(QStringLiteral("t2")), nullptr);
    QVERIFY(model.task(QStringLiteral("t3")));
    QCOMPARE(model.taskOfItem(QStringLiteral("i2")), nullptr);

    // Children first; the tasks in any order.
    QCOMPARE(recorder.events.size(), 3);
    QCOMPARE(recorder.events.last(), QStringLiteral("projectRemoved p1"));
    QVERIFY(recorder.events.contains(QStringLiteral("taskRemoved t1")));
    QVERIFY(recorder.events.contains(QStringLiteral("taskRemoved t2")));
}

void TestModelEdits::removeGroupCascades()
{
    Model model;
    fill(model);
    Recorder recorder(model);

    model.removeGroup(QStringLiteral("g1"));
    QCOMPARE(model.group(QStringLiteral("g1")), nullptr);
    QCOMPARE(model.project(QStringLiteral("p1")), nullptr);
    QCOMPARE(model.project(QStringLiteral("p2")), nullptr);
    QCOMPARE(model.task(QStringLiteral("t3")), nullptr);
    QVERIFY(model.group(QStringLiteral("g2")));
    QVERIFY(model.task(QStringLiteral("t4")));

    QCOMPARE(recorder.events.size(), 6);
    QCOMPARE(recorder.events.last(), QStringLiteral("groupRemoved g1"));
    // Each project's tasks come before the project.
    const auto at = [&recorder](const char *event) { return recorder.events.indexOf(QLatin1String(event)); };
    QVERIFY(at("taskRemoved t1") < at("projectRemoved p1"));
    QVERIFY(at("taskRemoved t2") < at("projectRemoved p1"));
    QVERIFY(at("taskRemoved t3") < at("projectRemoved p2"));
    QVERIFY(at("taskRemoved t1") >= 0 && at("taskRemoved t3") >= 0);
}

void TestModelEdits::removeUnknownEmitsNothing()
{
    Model model;
    fill(model);
    Recorder recorder(model);
    model.removeGroup(QStringLiteral("nope"));
    model.removeProject(QStringLiteral("nope"));
    model.removeTask(QStringLiteral("nope"));
    QVERIFY(recorder.events.isEmpty());
}

void TestModelEdits::removeTaskKeepsRootDashboards()
{
    Model model;
    fill(model);
    RootDashboard root;
    root.id = QStringLiteral("r1");
    root.layout = cell(CellRef{QString(), QStringLiteral("t1"), QString()});
    model.setRootDashboard(root);
    Recorder recorder(model);

    model.removeTask(QStringLiteral("t1"));
    QCOMPARE(recorder.events, QStringList({QStringLiteral("taskRemoved t1")}));
    const LayoutNode &layout = model.rootDashboard(QStringLiteral("r1"))->layout;
    QCOMPARE(layout.ref->task, QStringLiteral("t1"));
    QVERIFY(!model.refExists(*layout.ref));
}

void TestModelEdits::removeWithOwnId()
{
    // The id may be a reference into the entity being removed.
    Model model;
    fill(model);
    QSignalSpy spy(&model, &Model::groupRemoved);
    model.removeGroup(model.group(QStringLiteral("g1"))->id);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("g1"));

    QSignalSpy taskSpy(&model, &Model::taskRemoved);
    model.removeTask(model.task(QStringLiteral("t4"))->id);
    QCOMPARE(taskSpy.at(0).at(0).toString(), QStringLiteral("t4"));
}

QTEST_GUILESS_MAIN(TestModelEdits)
#include "tst_model_edits.moc"
