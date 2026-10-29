// Tests SidebarModel on the fixtures: structure, order, filters, roles and resets.
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
#include <QSignalSpy>
#include <QTabBar>
#include <QTest>
#include <QToolButton>
#include <QTreeView>

#include <memory>

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

// Drops id at row under parent and returns the move it asks for, as
// "kind id priority index", or "refused". canDropMimeData must agree.
QString drop(SidebarModel &sidebar, const QString &id, int row, const QModelIndex &parent)
{
    QSignalSpy spy(&sidebar, &SidebarModel::moveRequested);
    std::unique_ptr<QMimeData> data(sidebar.mimeData({sidebar.indexOf(id)}));
    const bool can = sidebar.canDropMimeData(data.get(), Qt::MoveAction, row, 0, parent);
    const bool dropped = sidebar.dropMimeData(data.get(), Qt::MoveAction, row, 0, parent);
    if (can != dropped || spy.count() != (dropped ? 1 : 0))
        return QStringLiteral("inconsistent");
    if (!dropped)
        return QStringLiteral("refused");
    const QList<QVariant> &args = spy.first();
    return QStringLiteral("%1 %2 %3 %4")
        .arg(int(args.at(0).value<SidebarModel::Kind>()))
        .arg(args.at(1).toString())
        .arg(args.at(2).toInt())
        .arg(args.at(3).toInt());
}

QString move(SidebarModel::Kind kind, const QString &id, int priority, int index)
{
    return QStringLiteral("%1 %2 %3 %4").arg(int(kind)).arg(id).arg(priority).arg(index);
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
    void followsModelEdits();
    void resetsOnModelReset();
    void repaintsChangedTask();
    void sidebarWidget();
    void emptyProjects();
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
    QVERIFY(!f.sidebar->filter().hideLow);
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
    filter.hideLow = true;
    f.sidebar->setFilter(filter);
    QVERIFY(f.sidebar->filter().hideLow);

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

    // Items and dashboards are not dropped in the sidebar.
    std::unique_ptr<QMimeData> itemDrop(f.sidebar->mimeData({item}));
    QVERIFY(!f.sidebar->canDropMimeData(itemDrop.get(), Qt::CopyAction, 0, 0, items));
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
    QCOMPARE(childIds(sidebar, project1), (QStringList{"a", "b", "c", "d"}));
    const auto task = SidebarModel::Kind::Task;
    const int high = int(Priority::High);
    const int normal = int(Priority::Normal);

    // Within a level: first, and after the next row.
    QCOMPARE(drop(sidebar, "b", 0, project1), move(task, "b", high, 0));
    QCOMPARE(drop(sidebar, "a", 2, project1), move(task, "a", high, 1));
    // Across a boundary, the row takes the level of the row above. The
    // hidden done task e counts in the index, after d.
    QCOMPARE(drop(sidebar, "a", 3, project1), move(task, "a", normal, 1));
    QCOMPARE(drop(sidebar, "a", 4, project1), move(task, "a", normal, 2));
    // Dropped first, it takes the level of the row below.
    QCOMPARE(drop(sidebar, "d", 0, project1), move(task, "d", high, 0));
    // A drop on a row of the same kind is a drop above it.
    QCOMPARE(drop(sidebar, "d", -1, sidebar.indexOf("c")), move(task, "d", high, 2));

    // Tasks stay in their project, and at their depth.
    QCOMPARE(drop(sidebar, "a", 0, sidebar.indexOf(p2.id)), QStringLiteral("refused"));
    QCOMPARE(drop(sidebar, "a", 0, {}), QStringLiteral("refused"));
    QCOMPARE(drop(sidebar, "a", 0, sidebar.indexOf(g1.id)), QStringLiteral("refused"));
    QCOMPARE(drop(sidebar, "a", -1, project1), QStringLiteral("refused"));
    QCOMPARE(drop(sidebar, "a", 0, sidebar.index(0, 0, sidebar.indexOf("c"))), QStringLiteral("refused"));

    // Projects among the projects of their group.
    const QModelIndex group1 = sidebar.indexOf(g1.id);
    QCOMPARE(drop(sidebar, "p2", 0, group1), move(SidebarModel::Kind::Project, "p2", normal, 0));
    QCOMPARE(drop(sidebar, "p1", 2, group1), move(SidebarModel::Kind::Project, "p1", normal, 1));
    QCOMPARE(drop(sidebar, "p3", 0, group1), QStringLiteral("refused"));
    QCOMPARE(drop(sidebar, "p1", 0, {}), QStringLiteral("refused"));

    // Groups among groups; the priority is unused.
    QCOMPARE(drop(sidebar, "g2", 0, {}), move(SidebarModel::Kind::Group, "g2", 0, 0));
    QCOMPARE(drop(sidebar, "g1", 2, {}), move(SidebarModel::Kind::Group, "g1", 0, 1));
    QCOMPARE(drop(sidebar, "g2", 0, group1), QStringLiteral("refused"));

    // A drop changes nothing itself.
    QCOMPARE(childIds(sidebar, {}), (QStringList{"g1", "g2"}));
    QCOMPARE(childIds(sidebar, project1), (QStringList{"a", "b", "c", "d"}));

    // The hint names the new level only when the priority changes.
    const auto hint = [&sidebar](const QString &id, int row, const QModelIndex &parent) {
        std::unique_ptr<QMimeData> data(sidebar.mimeData({sidebar.indexOf(id)}));
        return sidebar.dropHint(data.get(), row, parent);
    };
    QCOMPARE(hint("a", 3, project1), QStringLiteral("Becomes Normal"));
    QCOMPARE(hint("d", 0, project1), QStringLiteral("Becomes High"));
    QVERIFY(hint("a", 2, project1).isEmpty());
    QVERIFY(hint("a", 0, sidebar.indexOf(p2.id)).isEmpty());
    QVERIFY(hint("g2", 0, {}).isEmpty());
}

// The tree follows the model's setters and removers.
void TestSidebarModel::followsModelEdits()
{
    Fixture f;
    const QModelIndex backend = f.sidebar->indexOf(backendProject);
    QCOMPARE(childIds(*f.sidebar, backend).first(), fixLoginTimeout);

    // A changed priority moves the task.
    Task fix = *f.model.task(fixLoginTimeout);
    fix.priority = Priority::Low;
    fix.title = QStringLiteral("Fixed login timeout");
    f.model.setTask(fix);
    QStringList ids = childIds(*f.sidebar, f.sidebar->indexOf(backendProject));
    QCOMPARE(ids.first(), addRateLimiting);
    QVERIFY(ids.indexOf(fixLoginTimeout) > ids.indexOf(backendOverview));
    QCOMPARE(f.sidebar->indexOf(fixLoginTimeout).data().toString(), QStringLiteral("Fixed login timeout"));

    f.model.removeTask(addRateLimiting);
    QVERIFY(!f.sidebar->indexOf(addRateLimiting).isValid());

    // A renamed group is repainted.
    QSignalSpy changed(f.sidebar.get(), &QAbstractItemModel::dataChanged);
    Group work = *f.model.group(workGroup);
    work.name = QStringLiteral("Job");
    f.model.setGroup(work);
    QCOMPARE(f.sidebar->indexOf(workGroup).data().toString(), QStringLiteral("Job"));
    QCOMPARE(changed.count(), 1);

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

    // The Focus entry above the tree.
    auto *focus = sidebar.findChild<QPushButton *>(QStringLiteral("focusEntry"));
    QVERIFY(focus);
    QCOMPARE(focus->text(), QStringLiteral("Focus"));
    QSignalSpy focusRequested(&sidebar, &Sidebar::focusRequested);
    focus->click();
    QCOMPARE(focusRequested.count(), 1);

    const QString itemId = model.task(fixLoginTimeout)->items.front().id;
    sidebar.reveal(itemId);
    QCOMPARE(tree->currentIndex(), sidebarModel->indexOf(itemId));
    QVERIFY(tree->isExpanded(sidebarModel->indexOf(fixLoginTimeout)));

    QToolButton *hideLow = nullptr;
    for (QToolButton *button : sidebar.findChildren<QToolButton *>()) {
        if (button->isCheckable() && !button->isChecked())
            hideLow = button;
    }
    QVERIFY(hideLow);
    hideLow->click();
    QVERIFY(sidebarModel->filter().hideLow);

    // The current entry and the expanded task survive the reset.
    QCOMPARE(tree->currentIndex(), sidebarModel->indexOf(itemId));
    QVERIFY(tree->isExpanded(sidebarModel->indexOf(fixLoginTimeout)));

    // Three task lists: "Projects", "Active" without empty projects, "Done".
    auto *stateTabs = sidebar.findChild<QTabBar *>(QStringLiteral("taskStateTabs"));
    QVERIFY(stateTabs);
    QCOMPARE(stateTabs->count(), 3);
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

QTEST_MAIN(TestSidebarModel)
#include "tst_sidebar_model.moc"
