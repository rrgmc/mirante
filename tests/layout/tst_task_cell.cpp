// Tests LockState, and TaskCell on the fixtures: the nested view, the title,
// the lock button, the "open alone" button, and the header's menu and drag.
#include "core/Model.h"
#include "core/Store.h"
#include "items/ItemTypeRegistry.h"
#include "layout/DashboardCellFactory.h"
#include "layout/LayoutView.h"
#include "layout/LockState.h"
#include "layout/TaskCell.h"

#include <QApplication>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QJsonDocument>
#include <QLabel>
#include <QMouseEvent>
#include <QSignalSpy>
#include <QTest>
#include <QToolButton>

#include <memory>
#include <optional>

using namespace mirante;

namespace {

const QString kFixLoginTask = QStringLiteral("760c8e20-8555-5a2c-bcbe-75e7fd08ca74");
const QString kWorkDashboard = QStringLiteral("e7c480bf-fc19-50f3-81e4-18fc51f27f6c");     // default
const QString kReviewDashboard = QStringLiteral("ece7e850-6e0f-51ef-ad23-4ee948afdd66");
const QString kRateLimitTask = QStringLiteral("d71bd575-6d1b-5088-9da0-7d2cebf0d59e");    // one dashboard
const QString kRateLimitDashboard = QStringLiteral("4eeaa216-cc9a-5055-bca2-5d504578d4e1");
const QString kMorningRoot = QStringLiteral("60139a76-dd45-50ea-890b-ae022026e421");      // Fix login at {0}
const QString kReleaseRoot = QStringLiteral("5247a9e5-ee54-5552-8831-0b2ff361afba");

CellRef taskRef(const QString &task, const QString &dashboard = QString())
{
    return CellRef{QString(), task, dashboard};
}

QByteArray toJson(const LayoutNode &tree) { return QJsonDocument(layoutToJson(tree)).toJson(QJsonDocument::Compact); }

int cellCount(const LayoutView *view)
{
    int count = 0;
    for (const NodePath &path : layout::cellPaths(view->tree())) {
        if (view->cellAt(path))
            ++count;
    }
    return count;
}

QWidget *header(const TaskCell &cell) { return cell.findChild<QWidget *>(QStringLiteral("taskHeader")); }

// Presses the left button on widget and moves it by distance.
void dragBy(QWidget *widget, int distance)
{
    const QPoint start(10, 5);
    const QPoint end = start + QPoint(distance, 0);
    QTest::mousePress(widget, Qt::LeftButton, {}, start);
    QMouseEvent move(QEvent::MouseMove, QPointF(end), widget->mapToGlobal(QPointF(end)), Qt::NoButton,
                     Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(widget, &move);
    QTest::mouseRelease(widget, Qt::LeftButton, {}, end);
}

} // namespace

class TestTaskCell : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void lockState();
    void showsChosenDashboard();
    void fallsBackToDefaultDashboard();
    void titleNamesTaskAndDashboard();
    void followsModelChanges();
    void nestedViewIsReadOnly();
    void rootItemsNameTheirTask();
    void openAlone();
    void removedTask();
    void trashedTask();
    void headerMenuAndDrag();
    void rootViewForwardsTaskCells();

private:
    Model *m_model = nullptr;
    ItemTypeRegistry *m_types = nullptr;
    LockState *m_locks = nullptr;
    LayoutServices m_services;
};

void TestTaskCell::init()
{
    m_model = new Model;
    m_types = new ItemTypeRegistry;
    m_locks = new LockState;
    QVERIFY(store::load(QStringLiteral(MIRANTE_FIXTURES_DIR), *m_model).empty());
    m_services = LayoutServices();
    m_services.model = m_model;
    m_services.types = m_types;
    m_services.locks = m_locks;
}

void TestTaskCell::cleanup()
{
    delete m_locks;
    delete m_types;
    delete m_model;
}

void TestTaskCell::lockState()
{
    LockState locks;
    QSignalSpy spy(&locks, &LockState::changed);
    const QString id = QStringLiteral("dashboard");
    QVERIFY(!locks.isUnlocked(id));

    locks.setUnlocked(id, false);
    QCOMPARE(spy.count(), 0);

    locks.setUnlocked(id, true);
    QVERIFY(locks.isUnlocked(id));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), id);
    QCOMPARE(spy.at(0).at(1).toBool(), true);

    locks.setUnlocked(id, true);
    QCOMPARE(spy.count(), 1);
    QVERIFY(!locks.isUnlocked(QStringLiteral("other")));

    locks.setUnlocked(id, false);
    QVERIFY(!locks.isUnlocked(id));
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(1).at(1).toBool(), false);
}

void TestTaskCell::showsChosenDashboard()
{
    TaskCell cell(m_services, {QString(), kMorningRoot}, taskRef(kFixLoginTask, kReviewDashboard));
    QCOMPARE(cell.taskId(), kFixLoginTask);
    QCOMPARE(cell.dashboardId(), kReviewDashboard);
    QVERIFY(cell.view());
    QCOMPARE(toJson(cell.view()->tree()), toJson(*m_model->layout({kFixLoginTask, kReviewDashboard})));
    QCOMPARE(cellCount(cell.view()), 4);
    QCOMPARE(cell.findChild<QLabel *>(QStringLiteral("taskTitle"))->text(), QStringLiteral("Fix login timeout / Review"));
    QVERIFY(!cell.view()->editMode());
}

void TestTaskCell::fallsBackToDefaultDashboard()
{
    TaskCell empty(m_services, {QString(), kMorningRoot}, taskRef(kFixLoginTask));
    QCOMPARE(empty.dashboardId(), kWorkDashboard);
    QCOMPARE(toJson(empty.view()->tree()), toJson(*m_model->layout({kFixLoginTask, kWorkDashboard})));
    QCOMPARE(cellCount(empty.view()), 5);

    TaskCell unknown(m_services, {QString(), kMorningRoot}, taskRef(kFixLoginTask, QStringLiteral("nope")));
    QCOMPARE(unknown.dashboardId(), kWorkDashboard);
}

// A task cell shows one task dashboard and has no picker: its title names both.
void TestTaskCell::titleNamesTaskAndDashboard()
{
    TaskCell cell(m_services, {QString(), kReleaseRoot}, taskRef(kRateLimitTask, kRateLimitDashboard));
    const Task *task = m_model->task(kRateLimitTask);
    const Dashboard *dashboard = m_model->taskDashboard(kRateLimitTask, kRateLimitDashboard);
    QVERIFY(task && dashboard);
    QCOMPARE(cell.findChild<QLabel *>(QStringLiteral("taskTitle"))->text(),
             QStringLiteral("%1 / %2").arg(task->title, dashboard->name));
    QVERIFY(!cell.findChild<QComboBox *>());
    QCOMPARE(cellCount(cell.view()), int(layout::cellPaths(*m_model->layout({kRateLimitTask, kRateLimitDashboard})).size()));

    // The project follows the title, and follows a rename.
    auto *context = cell.findChild<QLabel *>(QStringLiteral("taskContext"));
    QVERIFY(context);
    Project project = *m_model->project(task->projectId);
    QCOMPARE(context->text(), project.name);
    project.name = QStringLiteral("Renamed project");
    m_model->setProject(project);
    QCOMPARE(context->text(), QStringLiteral("Renamed project"));
}

void TestTaskCell::followsModelChanges()
{
    TaskCell cell(m_services, {QString(), kMorningRoot}, taskRef(kFixLoginTask, kWorkDashboard));
    LayoutView *view = cell.view();
    QWidget *first = view->cellAt({0});
    QVERIFY(first);

    // Work is item | tabs[...]. Removing the tabs leaves the single item.
    const LayoutNode tree = *m_model->layout({kFixLoginTask, kWorkDashboard});
    m_model->setTaskDashboardLayout(kFixLoginTask, kWorkDashboard, layout::removeNode(tree, {1}));
    QCOMPARE(cell.view(), view);
    QCOMPARE(toJson(view->tree()), toJson(*m_model->layout({kFixLoginTask, kWorkDashboard})));
    QCOMPARE(cellCount(view), 1);
    QCOMPARE(view->cellAt({}), first);

    // A layout change of the other dashboard leaves the view alone.
    const LayoutNode review = *m_model->layout({kFixLoginTask, kReviewDashboard});
    m_model->setTaskDashboardLayout(kFixLoginTask, kReviewDashboard, layout::removeNode(review, {0}));
    QCOMPARE(cellCount(view), 1);

    // A reset loads the fixtures again.
    QVERIFY(store::load(QStringLiteral(MIRANTE_FIXTURES_DIR), *m_model).empty());
    QCOMPARE(cellCount(cell.view()), 5);
}

// A task dashboard is edited on its own, never inside a root dashboard: the
// task cell has no lock, and its view stays read-only even when the task
// dashboard is unlocked where it is open on its own.
void TestTaskCell::nestedViewIsReadOnly()
{
    TaskCell cell(m_services, {QString(), kMorningRoot}, taskRef(kFixLoginTask, kWorkDashboard));
    QVERIFY(!cell.findChild<QToolButton *>(QStringLiteral("lockButton")));
    QVERIFY(!cell.view()->editMode());
    m_locks->setUnlocked(kWorkDashboard, true);
    QVERIFY(!cell.view()->editMode());
    QVERIFY(!cell.view()->acceptDrops());
}

// An item placed directly in a root dashboard names its project and task in
// the header. Inside a task dashboard it does not, since the task is known.
void TestTaskCell::rootItemsNameTheirTask()
{
    const DashboardRef morning{QString(), kMorningRoot};
    std::optional<CellRef> itemRef;
    for (const NodePath &path : layout::cellPaths(*m_model->layout(morning))) {
        const LayoutNode *node = layout::nodeAt(*m_model->layout(morning), path);
        if (node->ref && node->ref->isItem()) {
            itemRef = node->ref;
            break;
        }
    }
    QVERIFY(itemRef);
    const Task *task = m_model->task(itemRef->task);
    const Project *project = m_model->project(task->projectId);

    DashboardCellFactory rootFactory(m_services, morning);
    std::unique_ptr<QWidget> rootCell(rootFactory.createCell(itemRef, false, nullptr));
    auto *context = rootCell->findChild<QLabel *>(QStringLiteral("cellContext"));
    QVERIFY(context);
    QCOMPARE(context->text(), QStringLiteral("%1 › %2").arg(project->name, task->title));

    DashboardCellFactory taskFactory(m_services, {task->id, task->defaultDashboardId});
    std::unique_ptr<QWidget> taskCell(taskFactory.createCell(CellRef{itemRef->item, QString(), QString()}, false, nullptr));
    QVERIFY(taskCell->findChild<QLabel *>(QStringLiteral("cellContext"))->text().isEmpty());
}

void TestTaskCell::openAlone()
{
    {
        TaskCell cell(m_services, {QString(), kMorningRoot}, taskRef(kFixLoginTask));
        QVERIFY(cell.findChild<QToolButton *>(QStringLiteral("openAloneButton"))->isHidden());
    }

    QStringList opened;
    m_services.openTaskAlone = [&opened](const QString &taskId, const QString &dashboardId) {
        opened.append(taskId + QLatin1Char('/') + dashboardId);
    };
    const QString expected = kFixLoginTask + QLatin1Char('/') + kReviewDashboard;
    TaskCell cell(m_services, {QString(), kMorningRoot}, taskRef(kFixLoginTask, kReviewDashboard));

    // The button opens the cell's own dashboard.
    QToolButton *button = cell.findChild<QToolButton *>(QStringLiteral("openAloneButton"));
    QVERIFY(button);
    QVERIFY(!button->isHidden());
    button->click();
    QCOMPARE(opened, QStringList{expected});

    // So does a double click, on the header or on a cell inside.
    QTest::mouseDClick(header(cell), Qt::LeftButton);
    QCOMPARE(opened.size(), 2);
    QTest::mouseDClick(cell.view()->cellAt(cell.view()->visibleCells().empty() ? NodePath{}
                                                                                 : *cell.view()->pathOf(cell.view()->visibleCells().front())),
                       Qt::LeftButton);
    QCOMPARE(opened.size(), 3);
    QCOMPARE(opened.back(), expected);
}

void TestTaskCell::removedTask()
{
    TaskCell cell(m_services, {QString(), kMorningRoot}, taskRef(QStringLiteral("no-such-task")));
    QVERIFY(!cell.view());
    auto *label = cell.findChild<QLabel *>(QStringLiteral("removedLabel"));
    QVERIFY(label);
    QVERIFY(!label->isHidden());
    QVERIFY(!header(cell)->isHidden());

    TaskCell present(m_services, {QString(), kMorningRoot}, taskRef(kFixLoginTask));
    QVERIFY(present.findChild<QLabel *>(QStringLiteral("removedLabel"))->isHidden());

    // An empty model removes the task.
    m_model->reset({}, {}, {}, {});
    QVERIFY(!present.view());
    QVERIFY(!present.findChild<QLabel *>(QStringLiteral("removedLabel"))->isHidden());

    // The next layout change normalises the cell into one without a
    // reference, marked removed. It still reads "Removed", not "Empty".
    DashboardCellFactory factory(m_services, {QString(), kMorningRoot});
    std::unique_ptr<QWidget> normalised(factory.createCell(std::nullopt, true, nullptr));
    QStringList texts;
    for (const QLabel *label : normalised->findChildren<QLabel *>())
        texts.append(label->text());
    QVERIFY(texts.contains(QStringLiteral("Removed")));
    QVERIFY(!texts.contains(QStringLiteral("Empty")));
}

// A task in the trash reads "In trash", keeps its title, and comes back when
// restored. So does an item of it in a root dashboard.
void TestTaskCell::trashedTask()
{
    TaskCell cell(m_services, {QString(), kMorningRoot}, taskRef(kFixLoginTask));
    auto *label = cell.findChild<QLabel *>(QStringLiteral("removedLabel"));
    QVERIFY(label && label->isHidden());

    Task task = *m_model->task(kFixLoginTask);
    task.trashedAt = QStringLiteral("2026-10-29T10:00:00Z");
    m_model->setTask(task);
    QVERIFY(!cell.view());
    QVERIFY(!label->isHidden());
    QCOMPARE(label->text(), QStringLiteral("In trash"));

    task.trashedAt.clear();
    m_model->setTask(task);
    QVERIFY(cell.view());
    QVERIFY(label->isHidden());

    // The task's project in the trash takes the task with it.
    Project project = *m_model->project(task.projectId);
    project.trashedAt = QStringLiteral("2026-10-29T10:00:00Z");
    m_model->setProject(project);
    QVERIFY(!label->isHidden());
    QCOMPARE(label->text(), QStringLiteral("In trash"));

    // An item of a task in the trash keeps its reference and reads "In trash".
    const DashboardRef morning{QString(), kMorningRoot};
    DashboardCellFactory factory(m_services, morning);
    const CellRef itemRef{task.items.front().id, task.id, QString()};
    QVERIFY(m_model->refExists(itemRef));
    std::unique_ptr<QWidget> itemCell(factory.createCell(itemRef, false, nullptr));
    QStringList texts;
    for (const QLabel *text : itemCell->findChildren<QLabel *>())
        texts.append(text->text());
    QVERIFY(texts.contains(QStringLiteral("In trash")));
    QVERIFY(texts.contains(task.items.front().name));
}

void TestTaskCell::headerMenuAndDrag()
{
    TaskCell cell(m_services, {QString(), kMorningRoot}, taskRef(kFixLoginTask));
    cell.resize(600, 400);
    cell.show();
    QVERIFY(QTest::qWaitForWindowExposed(&cell));
    QWidget *head = header(cell);
    QVERIFY(head);
    QSignalSpy menu(&cell, &TaskCell::menuRequested);
    QSignalSpy drag(&cell, &TaskCell::dragStarted);

    // While the root dashboard is locked, a right click on the header shows
    // the cell's own short menu, not the root cell menu. (Without an open
    // function the short menu has nothing to offer, so it does not open here.)
    const QPoint pos(5, 5);
    QContextMenuEvent locked(QContextMenuEvent::Mouse, pos, head->mapToGlobal(pos));
    QApplication::sendEvent(head, &locked);
    QCOMPARE(menu.count(), 0);

    // A drag starts only in the root dashboard's edit mode, and only past the drag distance.
    dragBy(head, 60);
    QCOMPARE(drag.count(), 0);
    cell.setEditMode(true);
    QCOMPARE(head->cursor().shape(), Qt::OpenHandCursor);

    // In the root dashboard's edit mode it asks for the root cell menu.
    QContextMenuEvent editing(QContextMenuEvent::Mouse, pos, head->mapToGlobal(pos));
    QApplication::sendEvent(head, &editing);
    QCOMPARE(menu.count(), 1);
    QCOMPARE(menu.at(0).at(0).toPoint(), head->mapToGlobal(pos));
    QVERIFY(!cell.view()->editMode());
    dragBy(head, 1);
    QCOMPARE(drag.count(), 0);
    dragBy(head, 60);
    QCOMPARE(drag.count(), 1);

    cell.setEditMode(false);
    QCOMPARE(head->cursor().shape(), Qt::ArrowCursor);
    dragBy(head, 60);
    QCOMPARE(drag.count(), 1);
}

void TestTaskCell::rootViewForwardsTaskCells()
{
    const DashboardRef root{QString(), kReleaseRoot};
    DashboardCellFactory factory(m_services, root);
    LayoutView rootView(&factory);
    rootView.setEditMode(true);
    rootView.setTree(*m_model->layout(root));
    QSignalSpy menu(&rootView, &LayoutView::cellMenuRequested);
    QSignalSpy drag(&rootView, &LayoutView::cellDragStarted);

    // Release is (three task cells) over (two item cells). New task cells start in edit mode.
    auto *cell = qobject_cast<TaskCell *>(rootView.cellAt({0, 2}));
    QVERIFY(cell);
    QCOMPARE(cell->taskId(), kFixLoginTask);
    QCOMPARE(header(*cell)->cursor().shape(), Qt::OpenHandCursor);
    QVERIFY(!cell->view()->editMode());

    emit cell->menuRequested(QPoint(5, 6));
    QCOMPARE(menu.count(), 1);
    QCOMPARE(menu.at(0).at(0).value<NodePath>(), (NodePath{0, 2}));
    QCOMPARE(menu.at(0).at(1).toPoint(), QPoint(5, 6));

    emit cell->dragStarted();
    QCOMPARE(drag.count(), 1);
    QCOMPARE(drag.at(0).at(0).value<NodePath>(), (NodePath{0, 2}));

    rootView.setEditMode(false);
    QCOMPARE(header(*cell)->cursor().shape(), Qt::ArrowCursor);
}

QTEST_MAIN(TestTaskCell)
#include "tst_task_cell.moc"
