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
#include <QSignalSpy>
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
const QString backendDefault = QStringLiteral("c34def7a-af34-507f-a44d-770bb60dbe95");
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
    void keepsDefaultTask();
    void roles();
    void findsIndexes();
    void dragging();
    void resetsOnModelReset();
    void repaintsChangedTask();
    void sidebarWidget();
};

void TestSidebarModel::structure()
{
    Fixture f;
    QCOMPARE(f.sidebar->rowCount(), 2);
    QCOMPARE(childIds(*f.sidebar, {}), (QStringList{workGroup, personalGroup}));

    const QModelIndex work = f.sidebar->index(0, 0);
    QCOMPARE(childIds(*f.sidebar, work), (QStringList{backendProject, frontendProject, infraProject}));
    QCOMPARE(f.sidebar->rowCount(f.sidebar->index(1, 0)), 2);

    const QModelIndex fix = f.sidebar->indexOf(fixLoginTimeout);
    QCOMPARE(f.sidebar->rowCount(fix), 8);
    QCOMPARE(f.sidebar->rowCount(f.sidebar->index(0, 0, fix)), 0);
}

void TestSidebarModel::taskOrder()
{
    Fixture f;
    const QModelIndex backend = f.sidebar->indexOf(backendProject);
    QCOMPARE(childIds(*f.sidebar, backend), (QStringList{backendDefault, fixLoginTimeout, addRateLimiting, flakyTest}));
}

void TestSidebarModel::hidesClosedByDefault()
{
    Fixture f;
    QVERIFY(f.sidebar->filter().hideClosed);
    QVERIFY(!f.sidebar->filter().hideLow);
    QVERIFY(!f.sidebar->indexOf(upgradeGo).isValid());

    QSignalSpy reset(f.sidebar.get(), &QAbstractItemModel::modelReset);
    SortFilter filter;
    filter.hideClosed = false;
    f.sidebar->setFilter(filter);
    QCOMPARE(reset.count(), 1);
    QCOMPARE(childIds(*f.sidebar, f.sidebar->indexOf(backendProject)),
             (QStringList{backendDefault, fixLoginTimeout, addRateLimiting, upgradeGo, flakyTest}));

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
             (QStringList{backendDefault, fixLoginTimeout, addRateLimiting}));
}

void TestSidebarModel::keepsDefaultTask()
{
    Group group;
    group.id = QStringLiteral("g");
    group.name = QStringLiteral("Group");
    Project project;
    project.id = QStringLiteral("p");
    project.groupId = group.id;
    project.name = QStringLiteral("Project");
    Task defaultTask;
    defaultTask.id = QStringLiteral("t");
    defaultTask.projectId = project.id;
    defaultTask.title = QStringLiteral("Default");
    defaultTask.isDefault = true;
    defaultTask.priority = Priority::Low;
    defaultTask.state = TaskState::Done;

    Model model;
    model.reset({group}, {project}, {defaultTask}, {});
    ItemTypeRegistry types;
    SidebarModel sidebar(model, types);
    QAbstractItemModelTester tester(&sidebar, QAbstractItemModelTester::FailureReportingMode::Fatal);

    SortFilter filter;
    filter.hideLow = true;
    filter.hideClosed = true;
    sidebar.setFilter(filter);
    QVERIFY(sidebar.indexOf(defaultTask.id).isValid());
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

    const QModelIndex item = f.sidebar->index(0, 0, fix);
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
    QCOMPARE(f.sidebar->indexOf(itemId).parent(), f.sidebar->indexOf(fixLoginTimeout));
    QVERIFY(!f.sidebar->indexOf(QStringLiteral("no-such-id")).isValid());
}

void TestSidebarModel::dragging()
{
    Fixture f;
    QCOMPARE(f.sidebar->mimeTypes(), QStringList{QString::fromLatin1(dragMimeType)});

    const QModelIndex group = f.sidebar->indexOf(workGroup);
    const QModelIndex task = f.sidebar->indexOf(fixLoginTimeout);
    const QModelIndex item = f.sidebar->index(0, 0, task);
    QVERIFY(!f.sidebar->flags(group).testFlag(Qt::ItemIsDragEnabled));
    QVERIFY(f.sidebar->flags(task).testFlag(Qt::ItemIsDragEnabled));
    QVERIFY(f.sidebar->flags(item).testFlag(Qt::ItemIsDragEnabled));

    std::unique_ptr<QMimeData> taskData(f.sidebar->mimeData({task}));
    QVERIFY(taskData);
    std::unique_ptr<QMimeData> itemData(f.sidebar->mimeData({item}));
    QVERIFY(itemData);
    QVERIFY(!f.sidebar->mimeData({group}));
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

    QCOMPARE(reset.count(), 0);
    QCOMPARE(changed.count(), 2);
    QCOMPARE(changed.at(0).at(0).value<QModelIndex>(), f.sidebar->indexOf(fixLoginTimeout));
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
}

QTEST_MAIN(TestSidebarModel)
#include "tst_sidebar_model.moc"
