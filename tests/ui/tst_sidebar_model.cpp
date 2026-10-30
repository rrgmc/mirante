// Tests SidebarModel on the fixtures: structure, order, filters, roles and
// resets. Attention counts use a small workspace of their own.
#include "AttentionTestSupport.h"

#include "core/GroupColours.h"
#include "core/Model.h"
#include "core/Store.h"
#include "items/ItemTypeRegistry.h"
#include "layout/DragMime.h"
#include "ui/Sidebar.h"
#include "ui/SidebarModel.h"

#include <QAbstractItemModelTester>
#include <QIcon>
#include <QMimeData>
#include <QPushButton>
#include <QMenu>
#include <QSignalSpy>
#include <QTabBar>
#include <QTest>
#include <QToolButton>
#include <QTreeView>

#include <memory>
#include <set>

using namespace mirante;

namespace {

const QString workGroup = QStringLiteral("6d1b9bb9-f4c3-5677-a4e1-683ba618d4c7");
const QString personalGroup = QStringLiteral("80314e00-c02f-5f8b-977f-0756b76406ea");
const QString backendProject = QStringLiteral("3dd79dc2-8ec9-5181-b7d0-704a18f494d5");
const QString frontendProject = QStringLiteral("fb99f6f7-be6d-5dd8-b3f2-2b09f71652a8");
const QString infraProject = QStringLiteral("475aa13b-e862-5f2e-b9aa-e1f4950328bc");
const QString backendOverview = QStringLiteral("c34def7a-af34-507f-a44d-770bb60dbe95");   // Normal, order 0
const QString fixLoginTimeout = QStringLiteral("760c8e20-8555-5a2c-bcbe-75e7fd08ca74");     // Urgent
const QString addRateLimiting = QStringLiteral("d71bd575-6d1b-5088-9da0-7d2cebf0d59e");     // High
const QString upgradeGo = QStringLiteral("493ef622-9528-5506-b3c2-a1f07b3de3d1");           // Normal, done
const QString flakyTest = QStringLiteral("02fc9a52-653c-562f-a3a2-6d5a4ca98623");           // Low

QStringList childIds(const SidebarModel &model, const QModelIndex &parent)
{
    QStringList ids;
    for (int row = 0; row < model.rowCount(parent); ++row)
        ids << model.index(row, 0, parent).data(SidebarModel::IdRole).toString();
    return ids;
}

// Drops the row dragged at row under parent and returns the move it asks for,
// as "kind id index", followed by " in taskId" for an item or a dashboard, or
// "refused". canDropMimeData must agree.
QString drop(SidebarModel &sidebar, const QModelIndex &dragged, int row, const QModelIndex &parent)
{
    QSignalSpy spy(&sidebar, &SidebarModel::moveRequested);
    std::unique_ptr<QMimeData> data(sidebar.mimeData({dragged}));
    const bool can = sidebar.canDropMimeData(data.get(), Qt::MoveAction, row, 0, parent);
    const bool dropped = sidebar.dropMimeData(data.get(), Qt::MoveAction, row, 0, parent);
    if (can != dropped || spy.count() != (dropped ? 1 : 0))
        return QStringLiteral("inconsistent");
    if (!dropped)
        return QStringLiteral("refused");
    const QList<QVariant> &args = spy.first();
    QString result = QStringLiteral("%1 %2 %3")
                         .arg(int(args.at(0).value<SidebarModel::Kind>()))
                         .arg(args.at(1).toString())
                         .arg(args.at(2).toInt());
    if (const QString taskId = args.at(3).toString(); !taskId.isEmpty())
        result += QStringLiteral(" in ") + taskId;
    return result;
}

QString drop(SidebarModel &sidebar, const QString &id, int row, const QModelIndex &parent)
{
    return drop(sidebar, sidebar.indexOf(id), row, parent);
}

QString move(SidebarModel::Kind kind, const QString &id, int index, const QString &taskId = {})
{
    QString result = QStringLiteral("%1 %2 %3").arg(int(kind)).arg(id).arg(index);
    if (!taskId.isEmpty())
        result += QStringLiteral(" in ") + taskId;
    return result;
}

// Loads the fixtures and checks the model with QAbstractItemModelTester.
struct Fixture
{
    Model model;
    ItemTypeRegistry types;
    std::unique_ptr<SidebarModel> sidebar;
    std::unique_ptr<QAbstractItemModelTester> tester;

    Fixture()
    {
        store::load(QStringLiteral(MIRANTE_FIXTURES_DIR), model);
        sidebar = std::make_unique<SidebarModel>(model, types);
        tester = std::make_unique<QAbstractItemModelTester>(
            sidebar.get(), QAbstractItemModelTester::FailureReportingMode::Fatal);
    }
};

} // namespace

class TestSidebarModel : public QObject
{
    Q_OBJECT

private slots:
    void structure();
    void taskOrder();
    void hidesClosedByDefault();
    void hidesLow();
    void roles();
    void findsIndexes();
    void dragging();
    void drops();
    void itemDrops();
    void followsModelEdits();
    void resetsOnModelReset();
    void repaintsChangedTask();
    void groupColours();
    void sidebarWidget();
    void emptyProjects();
    void trashList();
    void trashTab();
    void attentionRole();
};

void TestSidebarModel::structure()
{
    Fixture f;
    QCOMPARE(f.sidebar->rowCount(), 2);
    QCOMPARE(childIds(*f.sidebar, {}), (QStringList{workGroup, personalGroup}));

    const QModelIndex work = f.sidebar->index(0, 0);
    QCOMPARE(childIds(*f.sidebar, work), (QStringList{backendProject, frontendProject, infraProject}));
    QCOMPARE(f.sidebar->rowCount(f.sidebar->index(1, 0)), 2);

    // A task has two fixed children: its dashboards, then its items.
    const QModelIndex fix = f.sidebar->indexOf(fixLoginTimeout);
    QCOMPARE(f.sidebar->rowCount(fix), 2);
    const QModelIndex items = f.sidebar->index(1, 0, fix);
    const QModelIndex dashboards = f.sidebar->index(0, 0, fix);
    QCOMPARE(items.data(SidebarModel::KindRole).toInt(), int(SidebarModel::Kind::ItemList));
    QCOMPARE(dashboards.data(SidebarModel::KindRole).toInt(), int(SidebarModel::Kind::DashboardList));
    QCOMPARE(f.sidebar->rowCount(items), 8);
    QCOMPARE(f.sidebar->rowCount(dashboards), 2);
    QCOMPARE(f.sidebar->rowCount(f.sidebar->index(0, 0, items)), 0);
    QCOMPARE(f.sidebar->rowCount(f.sidebar->index(0, 0, dashboards)), 0);
}

void TestSidebarModel::taskOrder()
{
    Fixture f;
    const QModelIndex backend = f.sidebar->indexOf(backendProject);
    QCOMPARE(childIds(*f.sidebar, backend), (QStringList{fixLoginTimeout, addRateLimiting, backendOverview, flakyTest}));
}

void TestSidebarModel::hidesClosedByDefault()
{
    Fixture f;
    QVERIFY(!f.sidebar->filter().closedTasks);
    QVERIFY(f.sidebar->filter().hiddenPriorities.empty());
    QVERIFY(!f.sidebar->indexOf(upgradeGo).isValid());

    // The closed list shows the same groups and projects, with only done and
    // archived tasks.
    QSignalSpy reset(f.sidebar.get(), &QAbstractItemModel::modelReset);
    SortFilter filter;
    filter.closedTasks = true;
    f.sidebar->setFilter(filter);
    QCOMPARE(reset.count(), 1);
    QCOMPARE(childIds(*f.sidebar, {}), (QStringList{workGroup, personalGroup}));
    QCOMPARE(childIds(*f.sidebar, f.sidebar->index(0, 0)), (QStringList{backendProject, frontendProject, infraProject}));
    QCOMPARE(childIds(*f.sidebar, f.sidebar->indexOf(backendProject)), QStringList{upgradeGo});

    // Setting the same filter again changes nothing.
    f.sidebar->setFilter(filter);
    QCOMPARE(reset.count(), 1);
}

void TestSidebarModel::hidesLow()
{
    Fixture f;
    SortFilter filter;
    filter.hiddenPriorities = {Priority::Low};
    f.sidebar->setFilter(filter);
    QVERIFY(f.sidebar->filter().hiddenPriorities.count(Priority::Low) == 1);

    QCOMPARE(childIds(*f.sidebar, f.sidebar->index(0, 0)), (QStringList{backendProject, frontendProject}));
    QVERIFY(!f.sidebar->indexOf(infraProject).isValid());
    QCOMPARE(childIds(*f.sidebar, f.sidebar->indexOf(backendProject)),
             (QStringList{fixLoginTimeout, addRateLimiting, backendOverview}));
}

void TestSidebarModel::roles()
{
    Fixture f;
    const QModelIndex work = f.sidebar->indexOf(workGroup);
    QCOMPARE(work.data(Qt::DisplayRole).toString(), QStringLiteral("Work"));
    QCOMPARE(work.data(SidebarModel::KindRole).toInt(), int(SidebarModel::Kind::Group));
    QVERIFY(!work.data(SidebarModel::PriorityRole).isValid());
    QVERIFY(!work.data(SidebarModel::TaskIdRole).isValid());

    const QModelIndex backend = f.sidebar->indexOf(backendProject);
    QCOMPARE(backend.data(Qt::DisplayRole).toString(), QStringLiteral("backend"));
    QCOMPARE(backend.data(SidebarModel::KindRole).toInt(), int(SidebarModel::Kind::Project));
    QCOMPARE(backend.data(SidebarModel::PriorityRole).toInt(), int(Priority::High));

    const QModelIndex fix = f.sidebar->indexOf(fixLoginTimeout);
    QCOMPARE(fix.data(Qt::DisplayRole).toString(), QStringLiteral("Fix login timeout"));
    QCOMPARE(fix.data(SidebarModel::KindRole).toInt(), int(SidebarModel::Kind::Task));
    QCOMPARE(fix.data(SidebarModel::IdRole).toString(), fixLoginTimeout);
    QCOMPARE(fix.data(SidebarModel::TaskIdRole).toString(), fixLoginTimeout);
    QCOMPARE(fix.data(SidebarModel::PriorityRole).toInt(), int(Priority::Urgent));

    const QModelIndex items = f.sidebar->index(1, 0, fix);
    QCOMPARE(items.data(Qt::DisplayRole).toString(), QStringLiteral("Items"));
    QCOMPARE(items.data(SidebarModel::TaskIdRole).toString(), fixLoginTimeout);
    const QModelIndex dashboards = f.sidebar->index(0, 0, fix);
    QCOMPARE(dashboards.data(Qt::DisplayRole).toString(), QStringLiteral("Dashboards"));

    const QModelIndex dashboard = f.sidebar->index(0, 0, dashboards);
    const Dashboard &firstDashboard = f.model.task(fixLoginTimeout)->dashboards.front();
    QCOMPARE(dashboard.data(SidebarModel::KindRole).toInt(), int(SidebarModel::Kind::Dashboard));
    QCOMPARE(dashboard.data(SidebarModel::IdRole).toString(), firstDashboard.id);
    QCOMPARE(dashboard.data(SidebarModel::TaskIdRole).toString(), fixLoginTimeout);
    QCOMPARE(dashboard.data(Qt::DisplayRole).toString(), firstDashboard.name);
    QVERIFY(!dashboard.data(Qt::DecorationRole).value<QIcon>().isNull());

    const QModelIndex item = f.sidebar->index(0, 0, items);
    const Item &first = f.model.task(fixLoginTimeout)->items.front();
    QCOMPARE(item.data(SidebarModel::KindRole).toInt(), int(SidebarModel::Kind::Item));
    QCOMPARE(item.data(SidebarModel::IdRole).toString(), first.id);
    QCOMPARE(item.data(SidebarModel::TaskIdRole).toString(), fixLoginTimeout);
    QVERIFY(!item.data(SidebarModel::PriorityRole).isValid());
    QVERIFY(!item.data(Qt::DisplayRole).toString().isEmpty());
    QVERIFY(!item.data(Qt::DecorationRole).value<QIcon>().isNull());
}

void TestSidebarModel::findsIndexes()
{
    Fixture f;
    const QString itemId = f.model.task(fixLoginTimeout)->items.back().id;
    for (const QString &id : {workGroup, backendProject, fixLoginTimeout, itemId}) {
        const QModelIndex index = f.sidebar->indexOf(id);
        QVERIFY(index.isValid());
        QCOMPARE(index.data(SidebarModel::IdRole).toString(), id);
    }
    QCOMPARE(f.sidebar->indexOf(itemId).parent().parent(), f.sidebar->indexOf(fixLoginTimeout));
    QVERIFY(!f.sidebar->indexOf(QStringLiteral("no-such-id")).isValid());
}

void TestSidebarModel::dragging()
{
    Fixture f;
    QVERIFY(f.sidebar->mimeTypes().contains(QString::fromLatin1(dragMimeType)));

    // Everything but the two lists is dragged.
    const QModelIndex group = f.sidebar->indexOf(workGroup);
    const QModelIndex project = f.sidebar->indexOf(backendProject);
    const QModelIndex task = f.sidebar->indexOf(fixLoginTimeout);
    const QModelIndex items = f.sidebar->index(1, 0, task);
    const QModelIndex item = f.sidebar->index(0, 0, items);
    const QModelIndex dashboard = f.sidebar->index(0, 0, f.sidebar->index(0, 0, task));
    QVERIFY(!f.sidebar->flags(items).testFlag(Qt::ItemIsDragEnabled));
    QVERIFY(!f.sidebar->mimeData({items}));
    for (const QModelIndex &index : {group, project, task, item, dashboard})
        QVERIFY(f.sidebar->flags(index).testFlag(Qt::ItemIsDragEnabled));

    // Groups, projects and tasks only reorder the sidebar: dashboards do not
    // take them.
    for (const QModelIndex &index : {group, project, task}) {
        std::unique_ptr<QMimeData> data(f.sidebar->mimeData({index}));
        QVERIFY(data);
        QVERIFY(!data->hasFormat(QString::fromLatin1(dragMimeType)));
        QVERIFY(!dragPayload(data.get()));
    }

    // Items and dashboards are dropped only among their siblings; see itemDrops.
    std::unique_ptr<QMimeData> itemDrop(f.sidebar->mimeData({item}));
    QVERIFY(f.sidebar->canDropMimeData(itemDrop.get(), Qt::CopyAction, 0, 0, items));
    QVERIFY(!f.sidebar->canDropMimeData(itemDrop.get(), Qt::CopyAction, 0, 0, {}));

    std::unique_ptr<QMimeData> itemData(f.sidebar->mimeData({item}));
    QVERIFY(itemData);
    const std::optional<DragPayload> itemPayload = dragPayload(itemData.get());
    QVERIFY(itemPayload);
    QCOMPARE(itemPayload->ref.item, f.model.task(fixLoginTimeout)->items.front().id);
    QCOMPARE(itemPayload->ref.task, fixLoginTimeout);

    // A dashboard becomes a task cell that shows that dashboard.
    std::unique_ptr<QMimeData> dashboardData(f.sidebar->mimeData({dashboard}));
    const std::optional<DragPayload> dashboardPayload = dragPayload(dashboardData.get());
    QVERIFY(dashboardPayload);
    QVERIFY(dashboardPayload->ref.isTask());
    QCOMPARE(dashboardPayload->ref.task, fixLoginTimeout);
    QCOMPARE(dashboardPayload->ref.dashboard, f.model.task(fixLoginTimeout)->dashboards.front().id);
}

// Drops of groups, projects and tasks among their siblings, on a small model:
// groups g1 and g2; projects p1 and p2 in g1, p3 in g2; in p1 the tasks a and
// b (High), then c and d (Normal), and e (Normal, done, so hidden).
void TestSidebarModel::drops()
{
    Group g1;
    g1.id = QStringLiteral("g1");
    g1.order = 0;
    Group g2 = g1;
    g2.id = QStringLiteral("g2");
    g2.order = 1;
    Project p1;
    p1.id = QStringLiteral("p1");
    p1.groupId = g1.id;
    p1.order = 0;
    Project p2 = p1;
    p2.id = QStringLiteral("p2");
    p2.order = 1;
    Project p3 = p1;
    p3.id = QStringLiteral("p3");
    p3.groupId = g2.id;
    std::vector<Task> tasks;
    const auto addTask = [&tasks, &p1](const QString &id, Priority priority, int order) {
        Task task;
        task.id = id;
        task.projectId = p1.id;
        task.title = id;
        task.priority = priority;
        task.order = order;
        tasks.push_back(task);
    };
    addTask(QStringLiteral("a"), Priority::High, 0);
    addTask(QStringLiteral("b"), Priority::High, 1);
    addTask(QStringLiteral("c"), Priority::Normal, 0);
    addTask(QStringLiteral("d"), Priority::Normal, 1);
    addTask(QStringLiteral("e"), Priority::Normal, 2);
    tasks.back().state = TaskState::Done;

    Model model;
    model.reset({g1, g2}, {p1, p2, p3}, tasks, {});
    ItemTypeRegistry types;
    SidebarModel sidebar(model, types);
    QAbstractItemModelTester tester(&sidebar, QAbstractItemModelTester::FailureReportingMode::Fatal);
    QVERIFY(sidebar.supportedDropActions().testFlag(Qt::MoveAction));

    const QModelIndex project1 = sidebar.indexOf(p1.id);
    // The manual order alone: a and c have order 0, b and d order 1, and
    // equal orders go by title. Priority does not count.
    QCOMPARE(childIds(sidebar, project1), (QStringList{"a", "c", "b", "d"}));
    const auto task = SidebarModel::Kind::Task;

    // First, and after the next row, whatever the priorities around.
    QCOMPARE(drop(sidebar, "b", 0, project1), move(task, "b", 0));
    QCOMPARE(drop(sidebar, "a", 2, project1), move(task, "a", 1));
    // Last: the hidden done task e counts in the index, after d.
    QCOMPARE(drop(sidebar, "a", 4, project1), move(task, "a", 3));
    // A drop on a row of the same kind is a drop above it.
    QCOMPARE(drop(sidebar, "d", -1, sidebar.indexOf("c")), move(task, "d", 1));

    // Tasks stay in their project, and at their depth.
    QCOMPARE(drop(sidebar, "a", 0, sidebar.indexOf(p2.id)), QStringLiteral("refused"));
    QCOMPARE(drop(sidebar, "a", 0, {}), QStringLiteral("refused"));
    QCOMPARE(drop(sidebar, "a", 0, sidebar.indexOf(g1.id)), QStringLiteral("refused"));
    QCOMPARE(drop(sidebar, "a", -1, project1), QStringLiteral("refused"));
    QCOMPARE(drop(sidebar, "a", 0, sidebar.index(0, 0, sidebar.indexOf("c"))), QStringLiteral("refused"));

    // Projects among the projects of their group.
    const QModelIndex group1 = sidebar.indexOf(g1.id);
    QCOMPARE(drop(sidebar, "p2", 0, group1), move(SidebarModel::Kind::Project, "p2", 0));
    QCOMPARE(drop(sidebar, "p1", 2, group1), move(SidebarModel::Kind::Project, "p1", 1));
    QCOMPARE(drop(sidebar, "p3", 0, group1), QStringLiteral("refused"));
    QCOMPARE(drop(sidebar, "p1", 0, {}), QStringLiteral("refused"));

    // Groups among groups.
    QCOMPARE(drop(sidebar, "g2", 0, {}), move(SidebarModel::Kind::Group, "g2", 0));
    QCOMPARE(drop(sidebar, "g1", 2, {}), move(SidebarModel::Kind::Group, "g1", 1));
    QCOMPARE(drop(sidebar, "g2", 0, group1), QStringLiteral("refused"));

    // A drop changes nothing itself.
    QCOMPARE(childIds(sidebar, {}), (QStringList{"g1", "g2"}));
    QCOMPARE(childIds(sidebar, project1), (QStringList{"a", "c", "b", "d"}));
}

// Drops of items and dashboards among those of their own list, on a small
// model: tasks t1 and t2 in one project. t1 has items i1, i2, i3 and
// dashboards d1, d2; t2 has an item i1 of its own, with the same id.
void TestSidebarModel::itemDrops()
{
    Group group;
    group.id = QStringLiteral("g");
    Project project;
    project.id = QStringLiteral("p");
    project.groupId = group.id;
    const auto makeItem = [](const char *id) {
        Item item;
        item.id = QLatin1String(id);
        item.type = QStringLiteral("web.page");
        item.name = item.id;
        return item;
    };
    const auto makeDashboard = [](const char *id) {
        Dashboard dashboard;
        dashboard.id = QLatin1String(id);
        dashboard.name = dashboard.id;
        return dashboard;
    };
    Task t1;
    t1.id = QStringLiteral("t1");
    t1.projectId = project.id;
    t1.items = {makeItem("i1"), makeItem("i2"), makeItem("i3")};
    t1.dashboards = {makeDashboard("d1"), makeDashboard("d2")};
    t1.defaultDashboardId = QStringLiteral("d1");
    Task t2 = t1;
    t2.id = QStringLiteral("t2");
    t2.order = 1;
    t2.items = {makeItem("i1")};

    Model model;
    model.reset({group}, {project}, {t1, t2}, {});
    ItemTypeRegistry types;
    SidebarModel sidebar(model, types);
    QAbstractItemModelTester tester(&sidebar, QAbstractItemModelTester::FailureReportingMode::Fatal);

    const QModelIndex task1 = sidebar.indexOf(t1.id);
    const QModelIndex dashboards = sidebar.index(0, 0, task1);
    const QModelIndex items = sidebar.index(1, 0, task1);
    const QModelIndex otherItems = sidebar.index(1, 0, sidebar.indexOf(t2.id));
    QVERIFY(sidebar.flags(items).testFlag(Qt::ItemIsDropEnabled));
    QVERIFY(sidebar.flags(dashboards).testFlag(Qt::ItemIsDropEnabled));
    // The rows follow the order of the task's lists.
    QCOMPARE(childIds(sidebar, items), (QStringList{"i1", "i2", "i3"}));
    QCOMPARE(childIds(sidebar, dashboards), (QStringList{"d1", "d2"}));

    const QModelIndex i1 = sidebar.index(0, 0, items);
    const QModelIndex i3 = sidebar.index(2, 0, items);
    const auto item = SidebarModel::Kind::Item;
    QCOMPARE(drop(sidebar, i3, 0, items), move(item, "i3", 0, "t1"));
    QCOMPARE(drop(sidebar, i1, 2, items), move(item, "i1", 1, "t1"));
    QCOMPARE(drop(sidebar, i1, 3, items), move(item, "i1", 2, "t1"));
    // A drop on an item is a drop above it.
    QCOMPARE(drop(sidebar, i1, -1, i3), move(item, "i1", 1, "t1"));
    const QModelIndex d2 = sidebar.index(1, 0, dashboards);
    QCOMPARE(drop(sidebar, d2, 0, dashboards), move(SidebarModel::Kind::Dashboard, "d2", 0, "t1"));

    // The item of t2 with the same id is told apart by its task.
    const QModelIndex other = sidebar.index(0, 0, otherItems);
    QCOMPARE(drop(sidebar, other, 0, otherItems), move(item, "i1", 0, "t2"));
    QCOMPARE(drop(sidebar, other, 0, items), QStringLiteral("refused"));

    // Items stay in their own list in their own task, and dashboards in theirs.
    QCOMPARE(drop(sidebar, i1, 0, otherItems), QStringLiteral("refused"));
    QCOMPARE(drop(sidebar, i1, 0, dashboards), QStringLiteral("refused"));
    QCOMPARE(drop(sidebar, d2, 0, items), QStringLiteral("refused"));
    QCOMPARE(drop(sidebar, i1, -1, d2), QStringLiteral("refused"));
    QCOMPARE(drop(sidebar, i1, -1, items), QStringLiteral("refused"));
    QCOMPARE(drop(sidebar, i1, 0, task1), QStringLiteral("refused"));
    // Tasks are not dropped among items.
    QCOMPARE(drop(sidebar, task1, 0, items), QStringLiteral("refused"));

    // The drag still carries the reference that dashboards take.
    std::unique_ptr<QMimeData> data(sidebar.mimeData({i1}));
    const std::optional<DragPayload> payload = dragPayload(data.get());
    QVERIFY(payload);
    QCOMPARE(payload->ref.task, t1.id);
    QCOMPARE(payload->ref.item, QStringLiteral("i1"));

    // A drop changes nothing itself.
    QCOMPARE(childIds(sidebar, items), (QStringList{"i1", "i2", "i3"}));

    // A changed order in the task shows in the tree.
    Task moved = t1;
    std::swap(moved.items[0], moved.items[2]);
    model.setTask(moved);
    QCOMPARE(childIds(sidebar, sidebar.index(1, 0, sidebar.indexOf(t1.id))), (QStringList{"i3", "i2", "i1"}));
}

// The tree follows the model's setters and removers.
void TestSidebarModel::followsModelEdits()
{
    Fixture f;
    const QModelIndex backend = f.sidebar->indexOf(backendProject);
    QCOMPARE(childIds(*f.sidebar, backend).first(), fixLoginTimeout);

    // A changed priority and title leave the task where it is.
    Task fix = *f.model.task(fixLoginTimeout);
    fix.priority = Priority::Low;
    fix.title = QStringLiteral("Fixed login timeout");
    f.model.setTask(fix);
    QStringList ids = childIds(*f.sidebar, f.sidebar->indexOf(backendProject));
    QCOMPARE(ids.first(), fixLoginTimeout);
    QCOMPARE(f.sidebar->indexOf(fixLoginTimeout).data().toString(), QStringLiteral("Fixed login timeout"));

    f.model.removeTask(addRateLimiting);
    QVERIFY(!f.sidebar->indexOf(addRateLimiting).isValid());

    // A renamed group is repainted, and so are its project rows, which carry
    // its colour.
    QSignalSpy changed(f.sidebar.get(), &QAbstractItemModel::dataChanged);
    Group work = *f.model.group(workGroup);
    work.name = QStringLiteral("Job");
    f.model.setGroup(work);
    QCOMPARE(f.sidebar->indexOf(workGroup).data().toString(), QStringLiteral("Job"));
    QCOMPARE(changed.count(), 2);
    QCOMPARE(changed.at(0).at(0).value<QModelIndex>(), f.sidebar->indexOf(workGroup));

    // New projects and tasks appear; removed ones go.
    Project project;
    project.id = QStringLiteral("new-project");
    project.groupId = personalGroup;
    project.name = QStringLiteral("New");
    f.model.setProject(project);
    QCOMPARE(f.sidebar->indexOf(project.id).parent(), f.sidebar->indexOf(personalGroup));
    Task task;
    task.id = QStringLiteral("new-task");
    task.projectId = project.id;
    task.title = QStringLiteral("New task");
    f.model.setTask(task);
    QCOMPARE(f.sidebar->indexOf(task.id).parent(), f.sidebar->indexOf(project.id));
    f.model.removeProject(project.id);
    QVERIFY(!f.sidebar->indexOf(project.id).isValid());
    QVERIFY(!f.sidebar->indexOf(task.id).isValid());
    f.model.removeGroup(personalGroup);
    QCOMPARE(childIds(*f.sidebar, {}), QStringList{workGroup});
}

void TestSidebarModel::resetsOnModelReset()
{
    Fixture f;
    QSignalSpy reset(f.sidebar.get(), &QAbstractItemModel::modelReset);
    f.model.reset({}, {}, {}, {});
    QCOMPARE(reset.count(), 1);
    QCOMPARE(f.sidebar->rowCount(), 0);

    store::load(QStringLiteral(MIRANTE_FIXTURES_DIR), f.model);
    QCOMPARE(reset.count(), 2);
    QCOMPARE(f.sidebar->rowCount(), 2);
}

void TestSidebarModel::repaintsChangedTask()
{
    Fixture f;
    QSignalSpy reset(f.sidebar.get(), &QAbstractItemModel::modelReset);
    QSignalSpy changed(f.sidebar.get(), &QAbstractItemModel::dataChanged);

    const Task *task = f.model.task(fixLoginTimeout);
    f.model.setTaskDashboardLayout(fixLoginTimeout, task->dashboards.front().id, LayoutNode{});

    // The task and every row under it are repainted, and nothing else.
    QCOMPARE(reset.count(), 0);
    QVERIFY(changed.count() > 1);
    QCOMPARE(changed.at(0).at(0).value<QModelIndex>(), f.sidebar->indexOf(fixLoginTimeout));
    for (const QList<QVariant> &signal : changed)
        QCOMPARE(signal.at(0).value<QModelIndex>().data(SidebarModel::TaskIdRole).toString(), fixLoginTimeout);
}

// Groups and their projects carry the group colour: Work its palette colour,
// Personal the one it stores. A new colour repaints the group and its projects.
void TestSidebarModel::groupColours()
{
    Fixture f;
    const QModelIndex work = f.sidebar->indexOf(workGroup);
    const QModelIndex personal = f.sidebar->indexOf(personalGroup);
    const QModelIndex backend = f.sidebar->indexOf(backendProject);
    QCOMPARE(work.data(SidebarModel::GroupColourRole).value<QColor>(), QColor(groupcolours::fallback(workGroup)));
    QCOMPARE(personal.data(SidebarModel::GroupColourRole).value<QColor>(), QColor(QStringLiteral("#22c55e")));
    QCOMPARE(backend.data(SidebarModel::GroupColourRole).value<QColor>(), QColor(groupcolours::fallback(workGroup)));
    QVERIFY(!f.sidebar->indexOf(fixLoginTimeout).data(SidebarModel::GroupColourRole).isValid());

    QSignalSpy changed(f.sidebar.get(), &QAbstractItemModel::dataChanged);
    Group group = *f.model.group(workGroup);
    group.colour = QStringLiteral("#ff0000");
    f.model.setGroup(group);
    QCOMPARE(f.sidebar->indexOf(backendProject).data(SidebarModel::GroupColourRole).value<QColor>(),
             QColor(Qt::red));
    bool projectsRepainted = false;
    for (const QList<QVariant> &signal : changed) {
        const QModelIndex first = signal.at(0).value<QModelIndex>();
        const QModelIndex last = signal.at(1).value<QModelIndex>();
        if (first.parent() == f.sidebar->indexOf(workGroup) && first.row() == 0
            && last.row() == f.sidebar->rowCount(f.sidebar->indexOf(workGroup)) - 1)
            projectsRepainted = true;
    }
    QVERIFY(projectsRepainted);
}

// The widget: reveal selects the entry, and the filter buttons change the model.
void TestSidebarModel::sidebarWidget()
{
    Model model;
    store::load(QStringLiteral(MIRANTE_FIXTURES_DIR), model);
    ItemTypeRegistry types;
    Sidebar sidebar(model, types);

    auto *tree = sidebar.findChild<QTreeView *>();
    QVERIFY(tree);
    auto *sidebarModel = qobject_cast<SidebarModel *>(tree->model());
    QVERIFY(sidebarModel);

    // Shown, every row has a height. With uniform row heights the tree once
    // measured 0 and drew nothing.
    sidebar.resize(280, 600);
    sidebar.show();
    QVERIFY(QTest::qWaitForWindowExposed(&sidebar));
    QTRY_VERIFY(tree->visualRect(sidebarModel->index(0, 0)).height() > 0);
    QVERIFY(tree->visualRect(sidebarModel->index(1, 0)).top() > tree->visualRect(sidebarModel->index(0, 0)).top());

    // Focus has its slot in the tab row; the sidebar has no Focus entry.
    QVERIFY(!sidebar.findChild<QPushButton *>(QStringLiteral("focusEntry")));

    const QString itemId = model.task(fixLoginTimeout)->items.front().id;
    sidebar.reveal(itemId);
    QCOMPARE(tree->currentIndex(), sidebarModel->indexOf(itemId));
    QVERIFY(tree->isExpanded(sidebarModel->indexOf(fixLoginTimeout)));

    // The filter button right of the title: one checkbox for each priority,
    // all shown at first. Hiding one presses the button.
    auto *priorityFilter = sidebar.findChild<QToolButton *>(QStringLiteral("priorityFilter"));
    QVERIFY(priorityFilter && priorityFilter->menu());
    const QList<QAction *> levels = priorityFilter->menu()->actions();
    QCOMPARE(levels.size(), 4);
    QCOMPARE(levels.at(3)->text(), QStringLiteral("Low"));
    for (QAction *level : levels)
        QVERIFY(level->isChecked());
    QVERIFY(!priorityFilter->isChecked());
    levels.at(3)->trigger();
    QCOMPARE(sidebarModel->filter().hiddenPriorities, std::set<Priority>{Priority::Low});
    QVERIFY(priorityFilter->isChecked());

    // The current entry and the expanded task survive the reset.
    QCOMPARE(tree->currentIndex(), sidebarModel->indexOf(itemId));
    QVERIFY(tree->isExpanded(sidebarModel->indexOf(fixLoginTimeout)));

    // Four task lists: "Projects", "Active" without empty projects, "Done"
    // and "Trash".
    auto *stateTabs = sidebar.findChild<QTabBar *>(QStringLiteral("taskStateTabs"));
    QVERIFY(stateTabs);
    QCOMPARE(stateTabs->count(), 4);
    QCOMPARE(stateTabs->tabText(3), QStringLiteral("Trash"));
    QCOMPARE(stateTabs->currentIndex(), 0);
    stateTabs->setCurrentIndex(1);
    QVERIFY(sidebarModel->filter().hideEmptyProjects);
    QVERIFY(!sidebarModel->filter().closedTasks);
    stateTabs->setCurrentIndex(2);
    QVERIFY(sidebarModel->filter().closedTasks);
    QVERIFY(!sidebarModel->filter().hideEmptyProjects);
    QVERIFY(!sidebarModel->indexOf(fixLoginTimeout).isValid());

    // Revealing an active task switches to a list that has it.
    sidebar.reveal(fixLoginTimeout);
    QCOMPARE(stateTabs->currentIndex(), 0);
    QCOMPARE(tree->currentIndex(), sidebarModel->indexOf(fixLoginTimeout));

    // Expand all opens every task with its lists; collapse all closes the
    // tasks only, and leaves groups and projects open.
    sidebar.collapseAllTasks();
    QVERIFY(!tree->isExpanded(sidebarModel->indexOf(fixLoginTimeout)));
    QVERIFY(tree->isExpanded(sidebarModel->indexOf(backendProject)));
    sidebar.expandAllTasks();
    const QModelIndex task = sidebarModel->indexOf(addRateLimiting);
    QVERIFY(tree->isExpanded(task));
    QVERIFY(tree->isExpanded(sidebarModel->index(0, 0, task)));
    QVERIFY(tree->isExpanded(sidebarModel->index(1, 0, task)));
    sidebar.collapseAllTasks();
    QVERIFY(!tree->isExpanded(task));
    QVERIFY(tree->isExpanded(sidebarModel->indexOf(backendProject)));
    QVERIFY(tree->isExpanded(sidebarModel->indexOf(workGroup)));
}

// The "Active" list leaves out projects without active tasks; the others keep them.
void TestSidebarModel::emptyProjects()
{
    Group group;
    group.id = QStringLiteral("g");
    Project busy;
    busy.id = QStringLiteral("busy");
    busy.groupId = group.id;
    Project idle = busy;
    idle.id = QStringLiteral("idle");
    Task active;
    active.id = QStringLiteral("a");
    active.projectId = busy.id;
    Task done = active;
    done.id = QStringLiteral("d");
    done.projectId = idle.id;
    done.state = TaskState::Done;

    Model model;
    model.reset({group}, {busy, idle}, {active, done}, {});
    ItemTypeRegistry types;
    SidebarModel sidebar(model, types);

    QCOMPARE(childIds(sidebar, sidebar.index(0, 0)), (QStringList{busy.id, idle.id}));
    SortFilter filter;
    filter.hideEmptyProjects = true;
    sidebar.setFilter(filter);
    QCOMPARE(childIds(sidebar, sidebar.index(0, 0)), QStringList{busy.id});
    filter = SortFilter();
    filter.closedTasks = true;
    sidebar.setFilter(filter);
    QCOMPARE(childIds(sidebar, sidebar.index(0, 0)), (QStringList{busy.id, idle.id}));
    QVERIFY(sidebar.indexOf(done.id).isValid());
}


namespace {

// Two groups. In g1, project "kept" holds an active task "a" and a task "t" in
// the trash; project "gone" is in the trash with an active task "b" and a done
// task "c". Group g2 has nothing in the trash.
struct TrashFixture
{
    Model model;
    ItemTypeRegistry types;

    TrashFixture()
    {
        Group g1;
        g1.id = QStringLiteral("g1");
        Group g2 = g1;
        g2.id = QStringLiteral("g2");
        g2.order = 1;
        Project kept;
        kept.id = QStringLiteral("kept");
        kept.groupId = g1.id;
        Project gone = kept;
        gone.id = QStringLiteral("gone");
        gone.order = 1;
        gone.trashedAt = QStringLiteral("2026-10-29T10:00:00Z");
        Project other = kept;
        other.id = QStringLiteral("other");
        other.groupId = g2.id;
        Task a;
        a.id = QStringLiteral("a");
        a.projectId = kept.id;
        Task t = a;
        t.id = QStringLiteral("t");
        t.order = 1;
        t.trashedAt = QStringLiteral("2026-10-29T10:00:00Z");
        Task b = a;
        b.id = QStringLiteral("b");
        b.projectId = gone.id;
        Task c = b;
        c.id = QStringLiteral("c");
        c.order = 1;
        c.state = TaskState::Done;
        Task o = a;
        o.id = QStringLiteral("o");
        o.projectId = other.id;
        model.reset({g1, g2}, {kept, gone, other}, {a, t, b, c, o}, {});
    }
};

} // namespace

// The Trash list shows projects in the trash with all their tasks, and other
// projects only with their tasks in the trash. The other lists hide both.
void TestSidebarModel::trashList()
{
    TrashFixture f;
    SidebarModel sidebar(f.model, f.types);
    QAbstractItemModelTester tester(&sidebar, QAbstractItemModelTester::FailureReportingMode::Fatal);

    // The Projects list.
    QCOMPARE(childIds(sidebar, {}), (QStringList{QStringLiteral("g1"), QStringLiteral("g2")}));
    QCOMPARE(childIds(sidebar, sidebar.indexOf(QStringLiteral("g1"))), QStringList{QStringLiteral("kept")});
    QCOMPARE(childIds(sidebar, sidebar.indexOf(QStringLiteral("kept"))), QStringList{QStringLiteral("a")});
    QVERIFY(!sidebar.indexOf(QStringLiteral("b")).isValid());

    // The Done list leaves out the done task of the project in the trash.
    SortFilter filter;
    filter.closedTasks = true;
    sidebar.setFilter(filter);
    QVERIFY(!sidebar.indexOf(QStringLiteral("gone")).isValid());
    QVERIFY(!sidebar.indexOf(QStringLiteral("c")).isValid());

    // The Trash list: g2 has nothing there, so it is left out.
    filter = SortFilter();
    filter.trash = true;
    sidebar.setFilter(filter);
    QCOMPARE(childIds(sidebar, {}), QStringList{QStringLiteral("g1")});
    QCOMPARE(childIds(sidebar, sidebar.indexOf(QStringLiteral("g1"))),
             (QStringList{QStringLiteral("kept"), QStringLiteral("gone")}));
    QCOMPARE(childIds(sidebar, sidebar.indexOf(QStringLiteral("kept"))), QStringList{QStringLiteral("t")});
    QCOMPARE(childIds(sidebar, sidebar.indexOf(QStringLiteral("gone"))),
             (QStringList{QStringLiteral("b"), QStringLiteral("c")}));

    // Nothing in the trash is dragged, and nothing takes a drop.
    QVERIFY(!(sidebar.flags(sidebar.indexOf(QStringLiteral("t"))) & Qt::ItemIsDragEnabled));
    QVERIFY(!(sidebar.flags(sidebar.indexOf(QStringLiteral("gone"))) & Qt::ItemIsDropEnabled));
    QVERIFY(!(sidebar.flags({}) & Qt::ItemIsDropEnabled));

    // Restoring follows the model: the project leaves the trash with its tasks.
    Project gone = *f.model.project(QStringLiteral("gone"));
    gone.trashedAt.clear();
    f.model.setProject(gone);
    QCOMPARE(childIds(sidebar, sidebar.indexOf(QStringLiteral("g1"))), QStringList{QStringLiteral("kept")});
    filter = SortFilter();
    sidebar.setFilter(filter);
    QCOMPARE(childIds(sidebar, sidebar.indexOf(QStringLiteral("gone"))), QStringList{QStringLiteral("b")});
}

// The Trash tab sets the filter, offers "Empty Trash…", and uses its own menu.
void TestSidebarModel::trashTab()
{
    TrashFixture f;
    Sidebar sidebar(f.model, f.types);
    auto *stateTabs = sidebar.findChild<QTabBar *>(QStringLiteral("taskStateTabs"));
    auto *emptyTrash = sidebar.findChild<QPushButton *>(QStringLiteral("emptyTrash"));
    auto *tree = sidebar.findChild<QTreeView *>();
    QVERIFY(stateTabs && emptyTrash && tree);
    QVERIFY(emptyTrash->isHidden());
    QVERIFY(tree->dragEnabled());
    QVERIFY(!sidebar.showsTrash());

    stateTabs->setCurrentIndex(3);
    QVERIFY(sidebar.showsTrash());
    QVERIFY(sidebar.model()->filter().trash);
    QVERIFY(!emptyTrash->isHidden());
    QVERIFY(emptyTrash->isEnabled());
    QVERIFY(!tree->dragEnabled());
    QSignalSpy requested(&sidebar, &Sidebar::emptyTrashRequested);
    emptyTrash->click();
    QCOMPARE(requested.count(), 1);

    // An empty trash disables the button.
    Task task = *f.model.task(QStringLiteral("t"));
    task.trashedAt.clear();
    f.model.setTask(task);
    Project gone = *f.model.project(QStringLiteral("gone"));
    gone.trashedAt.clear();
    f.model.setProject(gone);
    QCOMPARE(sidebar.model()->rowCount(), 0);
    QVERIFY(!emptyTrash->isEnabled());

    stateTabs->setCurrentIndex(0);
    QVERIFY(emptyTrash->isHidden());
    QVERIFY(tree->dragEnabled());
}

// AttentionRole gives the counts of Attention for tasks, and their sums for
// projects and groups. A change repaints only the rows whose count changed.
void TestSidebarModel::attentionRole()
{
    attentiontest::Fixture f;
    ItemTypeRegistry types;
    SidebarModel sidebar(f.model, types);
    QAbstractItemModelTester tester(&sidebar, QAbstractItemModelTester::FailureReportingMode::Fatal);
    const auto count = [&sidebar](const char *id) {
        return sidebar.indexOf(QLatin1String(id)).data(SidebarModel::AttentionRole);
    };

    // Without Attention every count is 0, and items have none.
    QCOMPARE(count("g1"), QVariant(0));
    QCOMPARE(count("p1"), QVariant(0));
    QCOMPARE(count("t1"), QVariant(0));
    QVERIFY(!count("i1").isValid());

    sidebar.setAttention(f.attention.get());
    QSignalSpy changed(&sidebar, &QAbstractItemModel::dataChanged);
    // Two reasons on one item count once for its task.
    f.setReasons(QStringLiteral("i1"), {QStringLiteral("pipeline-failed"), QStringLiteral("conflicts")});
    f.setReasons(QStringLiteral("i3"), {QStringLiteral("pipeline-failed")});
    f.setReasons(QStringLiteral("i6"), {});
    // Attention reports the change on the next pass of the event loop.
    QTRY_COMPARE(changed.count(), 4);
    QCOMPARE(count("t1").toInt(), 1);
    QCOMPARE(count("t2").toInt(), 1);
    QCOMPARE(count("t3").toInt(), 0);
    QCOMPARE(count("t5").toInt(), 0);
    QCOMPARE(count("p1").toInt(), 2);
    QCOMPARE(count("p2").toInt(), 0);
    QCOMPARE(count("g1").toInt(), 2);
    QCOMPARE(count("g2").toInt(), 0);
    QVERIFY(sidebar.indexOf(QStringLiteral("t1")).data(Qt::ToolTipRole).toString().contains(
        QStringLiteral("need attention: 1")));

    QStringList repainted;
    for (const QList<QVariant> &args : changed) {
        QCOMPARE(args.at(0), args.at(1));
        QVERIFY(args.at(2).value<QList<int>>().contains(SidebarModel::AttentionRole));
        repainted << args.at(0).toModelIndex().data(SidebarModel::IdRole).toString();
    }
    repainted.sort();
    QCOMPARE(repainted, (QStringList{QStringLiteral("g1"), QStringLiteral("p1"), QStringLiteral("t1"),
                                     QStringLiteral("t2")}));

    // A count that drops repaints its rows again.
    changed.clear();
    f.setReasons(QStringLiteral("i3"), {});
    QTRY_COMPARE(changed.count(), 3);
    QCOMPARE(count("t2").toInt(), 0);
    QCOMPARE(count("p1").toInt(), 1);

    // Without Attention again, the counts go.
    changed.clear();
    sidebar.setAttention(nullptr);
    QCOMPARE(count("t1").toInt(), 0);
    QCOMPARE(count("g1").toInt(), 0);
    QCOMPARE(changed.count(), 3);
}

QTEST_MAIN(TestSidebarModel)
#include "tst_sidebar_model.moc"