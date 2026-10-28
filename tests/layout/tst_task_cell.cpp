// Tests LockState, and TaskCell on the fixtures: the nested view, the
// dashboard picker, the lock button, the "open alone" button, and storing the
// chosen dashboard in the root dashboard.
#include "core/Model.h"
#include "core/Store.h"
#include "items/ItemTypeRegistry.h"
#include "layout/DashboardCellFactory.h"
#include "layout/LayoutView.h"
#include "layout/LockState.h"
#include "layout/TaskCell.h"

#include <QComboBox>
#include <QJsonDocument>
#include <QLabel>
#include <QSignalSpy>
#include <QTest>
#include <QToolButton>

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

QComboBox *picker(const TaskCell &cell) { return cell.findChild<QComboBox *>(QStringLiteral("dashboardPicker")); }
QToolButton *lockButton(const TaskCell &cell) { return cell.findChild<QToolButton *>(QStringLiteral("lockButton")); }

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
    void pickerOnlyForSeveralDashboards();
    void followsModelChanges();
    void lockTogglesEditMode();
    void choosingDashboardUpdatesRoot();
    void choosingDashboardWithoutRoot();
    void openAlone();
    void removedTask();

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
    QCOMPARE(cell.findChild<QLabel *>(QStringLiteral("taskTitle"))->text(), m_model->task(kFixLoginTask)->title);
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

void TestTaskCell::pickerOnlyForSeveralDashboards()
{
    TaskCell two(m_services, {QString(), kMorningRoot}, taskRef(kFixLoginTask, kWorkDashboard));
    QComboBox *twoPicker = picker(two);
    QVERIFY(twoPicker);
    QVERIFY(!twoPicker->isHidden());
    QCOMPARE(twoPicker->count(), 2);
    QCOMPARE(twoPicker->currentData().toString(), kWorkDashboard);

    TaskCell one(m_services, {QString(), kReleaseRoot}, taskRef(kRateLimitTask, kRateLimitDashboard));
    QVERIFY(picker(one)->isHidden());
    QCOMPARE(cellCount(one.view()), int(layout::cellPaths(*m_model->layout({kRateLimitTask, kRateLimitDashboard})).size()));
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

void TestTaskCell::lockTogglesEditMode()
{
    TaskCell cell(m_services, {QString(), kMorningRoot}, taskRef(kFixLoginTask, kWorkDashboard));
    QToolButton *button = lockButton(cell);
    QVERIFY(button);
    QVERIFY(!button->isHidden());
    QVERIFY(!button->isChecked());
    QVERIFY(!cell.view()->editMode());

    button->click();
    QVERIFY(m_locks->isUnlocked(kWorkDashboard));
    QVERIFY(!m_locks->isUnlocked(kMorningRoot));
    QVERIFY(cell.view()->editMode());

    m_locks->setUnlocked(kWorkDashboard, false);
    QVERIFY(!button->isChecked());
    QVERIFY(!cell.view()->editMode());

    // Another dashboard's lock is its own.
    m_locks->setUnlocked(kReviewDashboard, true);
    QVERIFY(!cell.view()->editMode());
    picker(cell)->setCurrentIndex(picker(cell)->findData(kReviewDashboard));
    QVERIFY(cell.view()->editMode());
    QVERIFY(button->isChecked());
}

void TestTaskCell::choosingDashboardUpdatesRoot()
{
    const DashboardRef root{QString(), kMorningRoot};
    DashboardCellFactory factory(m_services, root);
    LayoutView rootView(&factory);
    rootView.setTree(*m_model->layout(root));

    auto *cell = qobject_cast<TaskCell *>(rootView.cellAt({0}));
    QVERIFY(cell);
    QCOMPARE(cell->dashboardId(), kWorkDashboard);
    QSignalSpy spy(m_model, &Model::rootDashboardChanged);

    QComboBox *cellPicker = picker(*cell);
    cellPicker->setCurrentIndex(cellPicker->findData(kReviewDashboard));
    QCOMPARE(cell->dashboardId(), kReviewDashboard);
    QCOMPARE(toJson(cell->view()->tree()), toJson(*m_model->layout({kFixLoginTask, kReviewDashboard})));

    QTRY_COMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), kMorningRoot);
    const LayoutNode *node = layout::nodeAt(*m_model->layout(root), {0});
    QVERIFY(node && node->ref);
    QCOMPARE(*node->ref, taskRef(kFixLoginTask, kReviewDashboard));

    // The owner passes the new tree back. The cell is made again with the new reference.
    rootView.setTree(*m_model->layout(root));
    auto *newCell = qobject_cast<TaskCell *>(rootView.cellAt({0}));
    QVERIFY(newCell);
    QCOMPARE(newCell->dashboardId(), kReviewDashboard);
}

void TestTaskCell::choosingDashboardWithoutRoot()
{
    const QByteArray before = toJson(*m_model->layout({QString(), kMorningRoot}));
    TaskCell cell(m_services, {QString(), kMorningRoot}, taskRef(kFixLoginTask, kWorkDashboard));
    QSignalSpy spy(m_model, &Model::rootDashboardChanged);
    picker(cell)->setCurrentIndex(picker(cell)->findData(kReviewDashboard));
    QCOMPARE(cell.dashboardId(), kReviewDashboard);
    QCOMPARE(cellCount(cell.view()), 4);
    QTest::qWait(20);
    QCOMPARE(spy.count(), 0);
    QCOMPARE(toJson(*m_model->layout({QString(), kMorningRoot})), before);
}

void TestTaskCell::openAlone()
{
    {
        TaskCell cell(m_services, {QString(), kMorningRoot}, taskRef(kFixLoginTask));
        QVERIFY(cell.findChild<QToolButton *>(QStringLiteral("openAloneButton"))->isHidden());
    }

    QStringList opened;
    m_services.openTaskAlone = [&opened](const QString &taskId) { opened.append(taskId); };
    TaskCell cell(m_services, {QString(), kMorningRoot}, taskRef(kFixLoginTask));
    QToolButton *button = cell.findChild<QToolButton *>(QStringLiteral("openAloneButton"));
    QVERIFY(button);
    QVERIFY(!button->isHidden());
    button->click();
    QCOMPARE(opened, QStringList{kFixLoginTask});
}

void TestTaskCell::removedTask()
{
    TaskCell cell(m_services, {QString(), kMorningRoot}, taskRef(QStringLiteral("no-such-task")));
    QVERIFY(!cell.view());
    auto *label = cell.findChild<QLabel *>(QStringLiteral("removedLabel"));
    QVERIFY(label);
    QVERIFY(!label->isHidden());

    TaskCell present(m_services, {QString(), kMorningRoot}, taskRef(kFixLoginTask));
    QVERIFY(present.findChild<QLabel *>(QStringLiteral("removedLabel"))->isHidden());

    // An empty model removes the task.
    m_model->reset({}, {}, {}, {});
    QVERIFY(!present.view());
    QVERIFY(!present.findChild<QLabel *>(QStringLiteral("removedLabel"))->isHidden());
}

QTEST_MAIN(TestTaskCell)
#include "tst_task_cell.moc"
