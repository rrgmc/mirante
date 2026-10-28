// Tests MainWindow on the fixtures: the root tabs, the preview tab, pinning,
// new root dashboards, closing and reopening tabs, the warning bar, focusing an
// item, and views following the model.
#include "core/Model.h"
#include "core/Store.h"
#include "items/ItemTypeRegistry.h"
#include "layout/CellFrame.h"
#include "layout/LayoutView.h"
#include "ui/ItemWindow.h"
#include "ui/MainWindow.h"
#include "ui/RootDashboardsDialog.h"
#include "ui/RootTabBar.h"
#include "ui/Sidebar.h"
#include "ui/SidebarModel.h"

#include <QApplication>
#include <QLabel>
#include <QPointer>
#include <QSignalSpy>
#include <QTest>
#include <QToolButton>
#include <QTreeView>

#include <memory>

using namespace mirante;

namespace {

const QString kMorning = QStringLiteral("60139a76-dd45-50ea-890b-ae022026e421");
const QString kRelease = QStringLiteral("5247a9e5-ee54-5552-8831-0b2ff361afba");
const QString kFixLoginTask = QStringLiteral("760c8e20-8555-5a2c-bcbe-75e7fd08ca74");
const QString kFixLoginDashboard = QStringLiteral("e7c480bf-fc19-50f3-81e4-18fc51f27f6c");
const QString kOverviewTask = QStringLiteral("c34def7a-af34-507f-a44d-770bb60dbe95");
const QString kReviewsItem = QStringLiteral("191057b6-8355-5994-beae-62a500d35f8d");

// The registry has no modules, so every item has an unknown type.
struct Fixture;
QString overviewTitle(const Fixture &f);

struct Fixture
{
    ItemTypeRegistry types;
    Model model;
    std::unique_ptr<MainWindow> window;

    Fixture()
    {
        const auto problems = store::load(QStringLiteral(MIRANTE_FIXTURES_DIR), model);
        if (!problems.empty())
            qWarning() << "fixture problems:" << problems.size();
        window = std::make_unique<MainWindow>(model, types);
    }

    RootTabBar *bar() const { return window->rootTabBar(); }
    QString tabText(int index) const { return bar()->tabText(index); }
};

// The tab title of the Overview task's default dashboard: "Task / Dashboard".
QString overviewTitle(const Fixture &f)
{
    const Task *task = f.model.task(kOverviewTask);
    return QStringLiteral("%1 / %2").arg(task->title, f.model.taskDashboard(kOverviewTask, task->defaultDashboardId)->name);
}

} // namespace

class TestMainWindow : public QObject
{
    Q_OBJECT

private slots:
    void rootTabsAtStart()
    {
        Fixture f;
        QCOMPARE(f.bar()->count(), 2);
        QCOMPARE(f.tabText(0), QStringLiteral("Morning"));
        QCOMPARE(f.tabText(1), QStringLiteral("Release"));
        QCOMPARE(f.bar()->previewIndex(), -1);
        QCOMPARE(f.bar()->currentIndex(), 0);
        QVERIFY(f.window->currentLayoutView());
        QCOMPARE(f.window->currentLayoutView()->tree().kind, LayoutNode::Kind::Split);
        QVERIFY(f.window->contentArea());
        QVERIFY(f.window->shortcuts());
    }

    void previewTab()
    {
        Fixture f;
        f.window->openTaskPreview(kFixLoginTask);
        QCOMPARE(f.bar()->count(), 3);
        QCOMPARE(f.bar()->previewIndex(), 2);
        QCOMPARE(f.bar()->currentIndex(), 2);
        QCOMPARE(f.tabText(2), QStringLiteral("Fix login timeout / Work"));
        QVERIFY(f.bar()->findChild<QToolButton *>(QStringLiteral("pinButton")));
        f.window->show();
        QVERIFY(QTest::qWaitForWindowExposed(f.window.get()));
        QTRY_VERIFY(f.bar()->height() > 0);
        QVERIFY(!f.bar()->grab().isNull());     // paints the italic tab

        const LayoutNode *expected = f.model.layout(DashboardRef{kFixLoginTask, kFixLoginDashboard});
        QVERIFY(expected);
        QVERIFY(layout::sameShape(f.window->currentLayoutView()->tree(), *expected));

        // A second preview replaces the first.
        f.window->openTaskPreview(kOverviewTask);
        QCOMPARE(f.bar()->count(), 3);
        QCOMPARE(f.bar()->previewIndex(), 2);
        QCOMPARE(f.tabText(2), overviewTitle(f));

        // Closing the preview removes its tab.
        f.window->closeCurrentTab();
        QCOMPARE(f.bar()->count(), 2);
        QCOMPARE(f.bar()->previewIndex(), -1);
    }

    void previewOfAChosenDashboard()
    {
        Fixture f;
        // A chosen dashboard replaces the default one in the same preview tab,
        // and pinning it pins that dashboard.
        const QString review = QStringLiteral("ece7e850-6e0f-51ef-ad23-4ee948afdd66");
        f.window->openTaskPreview(kFixLoginTask);
        f.window->openTaskPreview(kFixLoginTask, review);
        QCOMPARE(f.bar()->count(), 3);
        QCOMPARE(f.bar()->previewIndex(), 2);
        const LayoutNode *reviewTree = f.model.layout(DashboardRef{kFixLoginTask, review});
        QVERIFY(reviewTree);
        QVERIFY(layout::sameShape(f.window->currentLayoutView()->tree(), *reviewTree));
        f.window->pinTask(kFixLoginTask, review);
        QCOMPARE(f.bar()->previewIndex(), -1);
        QVERIFY(layout::sameShape(f.window->currentLayoutView()->tree(), *reviewTree));
        f.window->openTaskPreview(kFixLoginTask);
        QCOMPARE(f.bar()->previewIndex(), 3);      // the default dashboard is not pinned yet
    }

    // Pinning keeps a task dashboard in its own tab. It does not make a root
    // dashboard: a task dashboard in a root dashboard is read-only.
    void pinTask()
    {
        Fixture f;
        f.window->openTaskPreview(kFixLoginTask);
        f.window->pinTask(kFixLoginTask);

        QCOMPARE(int(f.model.rootDashboards().size()), 2);
        QCOMPARE(f.bar()->count(), 3);
        QCOMPARE(f.bar()->previewIndex(), -1);
        QCOMPARE(f.bar()->currentIndex(), 2);
        QCOMPARE(f.tabText(2), QStringLiteral("Fix login timeout / Work"));
        QVERIFY(!f.bar()->tabIcon(2).isNull());    // a task dashboard tab
        QVERIFY(f.bar()->tabIcon(0).isNull());     // a root dashboard tab
        const LayoutNode *work = f.model.layout(DashboardRef{kFixLoginTask, kFixLoginDashboard});
        QVERIFY(layout::sameShape(f.window->currentLayoutView()->tree(), *work));

        // Opening it again switches to its tab.
        f.window->openRootDashboard(kRelease);
        QCOMPARE(f.bar()->currentIndex(), 1);
        f.window->openTaskPreview(kFixLoginTask);
        QCOMPARE(f.bar()->currentIndex(), 2);
        QCOMPARE(f.bar()->count(), 3);
        QCOMPARE(f.bar()->previewIndex(), -1);

        // The next preview opens in a new tab after it.
        f.window->openTaskPreview(kOverviewTask);
        QCOMPARE(f.bar()->count(), 4);
        QCOMPARE(f.bar()->previewIndex(), 3);
    }

    // A task dashboard tab has its own lock in its title bar. The lock at the
    // end of the tab bar row is for root dashboards only.
    void taskTabHasItsOwnLock()
    {
        Fixture f;
        f.window->show();
        QVERIFY(QTest::qWaitForWindowExposed(f.window.get()));
        QToolButton *rootLock = nullptr;
        for (QToolButton *button : f.window->findChildren<QToolButton *>(QStringLiteral("lockButton")))
            rootLock = button;
        QVERIFY(rootLock);
        QVERIFY(rootLock->isVisible());

        f.window->openTaskPreview(kFixLoginTask);
        QVERIFY(!rootLock->isVisible());

        // The sidebar selects the dashboard on screen, under its task.
        auto *tree = f.window->findChild<Sidebar *>()->findChild<QTreeView *>();
        QVERIFY(tree);
        QCOMPARE(tree->currentIndex().data(SidebarModel::IdRole).toString(), kFixLoginDashboard);
        QCOMPARE(tree->currentIndex().data(SidebarModel::TaskIdRole).toString(), kFixLoginTask);
        QWidget *page = f.window->currentLayoutView()->parentWidget();
        auto *taskLock = page->findChild<QToolButton *>(QStringLiteral("taskLockButton"));
        QVERIFY(taskLock);
        QVERIFY(taskLock->isVisible());
        QVERIFY(!f.window->currentLayoutView()->editMode());
        taskLock->click();
        QVERIFY(f.window->currentLayoutView()->editMode());

        f.window->openRootDashboard(kMorning);
        QVERIFY(rootLock->isVisible());
        QVERIFY(!f.window->currentLayoutView()->editMode());
    }

    void editingPreviewPinsIt()
    {
        Fixture f;
        f.window->openTaskPreview(kFixLoginTask);
        QCOMPARE(f.bar()->previewIndex(), 2);

        // An edit of the previewed task dashboard pins the preview.
        const DashboardRef ref{kFixLoginTask, kFixLoginDashboard};
        const LayoutNode *tree = f.model.layout(ref);
        QVERIFY(tree);
        f.model.setLayout(ref, layout::splitCell(*tree, {}, Edge::Right, std::nullopt).tree);

        QTRY_COMPARE(f.bar()->previewIndex(), -1);
        QCOMPARE(int(f.model.rootDashboards().size()), 2);
        QCOMPARE(f.bar()->count(), 3);
        QCOMPARE(f.tabText(f.bar()->currentIndex()), QStringLiteral("Fix login timeout / Work"));
    }

    void pinButton()
    {
        Fixture f;
        f.window->openTaskPreview(kOverviewTask);
        auto *pin = f.bar()->findChild<QToolButton *>(QStringLiteral("pinButton"));
        QVERIFY(pin);
        QSignalSpy spy(f.bar(), &RootTabBar::pinRequested);
        pin->click();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 2);
        QCOMPARE(f.bar()->previewIndex(), -1);
        QCOMPARE(f.bar()->count(), 3);
        QCOMPARE(f.tabText(2), overviewTitle(f));
        QCOMPARE(int(f.model.rootDashboards().size()), 2);
    }

    void newRootDashboard()
    {
        Fixture f;
        f.window->openTaskPreview(kFixLoginTask);
        f.window->newRootDashboard();
        QCOMPARE(int(f.model.rootDashboards().size()), 3);
        QCOMPARE(f.bar()->count(), 4);
        // Root tabs come before the preview.
        QCOMPARE(f.bar()->currentIndex(), 2);
        QCOMPARE(f.bar()->previewIndex(), 3);
        QCOMPARE(f.tabText(2), QStringLiteral("New dashboard"));

        LayoutView *view = f.window->currentLayoutView();
        QVERIFY(view->editMode());
        QCOMPARE(view->tree().kind, LayoutNode::Kind::Cell);
        QVERIFY(!view->tree().ref);

        // The + button asks for one too.
        auto *add = f.window->findChild<QToolButton *>(QStringLiteral("newDashboardButton"));
        QVERIFY(add);
        add->click();
        QCOMPARE(int(f.model.rootDashboards().size()), 4);
    }

    void closeAndReopen()
    {
        Fixture f;
        QCOMPARE(f.bar()->currentIndex(), 0);
        f.window->closeCurrentTab();
        QCOMPARE(f.bar()->count(), 1);
        QCOMPARE(f.tabText(0), QStringLiteral("Release"));
        QCOMPARE(int(f.model.rootDashboards().size()), 2);

        f.window->openRootDashboard(kMorning);
        QCOMPARE(f.bar()->count(), 2);
        QCOMPARE(f.tabText(0), QStringLiteral("Morning"));
        QCOMPARE(f.bar()->currentIndex(), 0);

        // The tab's close button does the same.
        emit f.bar()->tabCloseRequested(1);
        QCOMPARE(f.bar()->count(), 1);
        QCOMPARE(f.tabText(0), QStringLiteral("Morning"));
    }

    void warningBar()
    {
        Fixture f;
        auto *bar = f.window->findChild<QFrame *>(QStringLiteral("warningBar"));
        QVERIFY(bar);
        QVERIFY(!bar->isVisibleTo(f.window.get()));

        f.window->setLoadProblems({LoadProblem{QStringLiteral("tasks/broken.json"), QStringLiteral("Not JSON")}});
        QVERIFY(bar->isVisibleTo(f.window.get()));
        const auto labels = bar->findChildren<QLabel *>();
        QVERIFY(!labels.isEmpty());
        QVERIFY(labels.front()->text().contains(QStringLiteral("tasks/broken.json")));

        f.window->setLoadProblems({});
        QVERIFY(!bar->isVisibleTo(f.window.get()));
    }

    void viewsFollowModel()
    {
        Fixture f;
        LayoutView *morning = f.window->currentLayoutView();
        f.model.setRootDashboardLayout(kMorning, layout::emptyLayout());
        QCOMPARE(morning->tree().kind, LayoutNode::Kind::Cell);
        QVERIFY(!morning->tree().ref);

        f.window->openTaskPreview(kFixLoginTask);
        LayoutView *preview = f.window->currentLayoutView();
        QVERIFY(preview != morning);
        f.model.setTaskDashboardLayout(kFixLoginTask, kFixLoginDashboard, layout::emptyLayout());
        QCOMPARE(preview->tree().kind, LayoutNode::Kind::Cell);
    }

    void sidebarCollapse()
    {
        Fixture f;
        auto *sidebar = f.window->findChild<Sidebar *>();
        auto *show = f.window->findChild<QToolButton *>(QStringLiteral("showSidebarButton"));
        QVERIFY(sidebar && show);
        QVERIFY(sidebar->isVisibleTo(f.window.get()));
        emit sidebar->collapseRequested();
        QVERIFY(!sidebar->isVisibleTo(f.window.get()));
        QVERIFY(show->isVisibleTo(f.window.get()));
        show->click();
        QVERIFY(sidebar->isVisibleTo(f.window.get()));
        QVERIFY(!show->isVisibleTo(f.window.get()));
    }

    // A middle click closes a tab. Its root dashboard is kept.
    void middleClickClosesTab()
    {
        Fixture f;
        f.window->show();
        QVERIFY(QTest::qWaitForWindowExposed(f.window.get()));
        QTRY_VERIFY(f.bar()->height() > 0);
        QTest::mouseClick(f.bar(), Qt::MiddleButton, {}, f.bar()->tabRect(0).center());
        QCOMPARE(f.bar()->count(), 1);
        QCOMPARE(f.tabText(0), QStringLiteral("Release"));
        QVERIFY(f.model.rootDashboard(kMorning));
    }

    // The list holds every root dashboard. It opens closed ones again, and is
    // the only place where one is deleted.
    void dashboardList()
    {
        Fixture f;
        f.window->closeCurrentTab();    // closes Morning
        QCOMPARE(f.bar()->count(), 1);

        f.window->showDashboardList();
        auto *list = f.window->findChild<RootDashboardsDialog *>();
        QVERIFY(list);
        QCOMPARE(list->ids(), (QStringList{kMorning, kRelease}));

        list->select(kMorning);
        list->openSelected();
        QCOMPARE(f.bar()->count(), 2);
        QCOMPARE(f.tabText(f.bar()->currentIndex()), QStringLiteral("Morning"));

        // Deleting asks first. Saying no keeps the dashboard.
        QString asked;
        list->setConfirmDelete([&asked](const QString &name) {
            asked = name;
            return false;
        });
        list->deleteSelected();
        QCOMPARE(asked, QStringLiteral("Morning"));
        QVERIFY(f.model.rootDashboard(kMorning));

        list->setConfirmDelete([](const QString &) { return true; });
        list->deleteSelected();
        QVERIFY(!f.model.rootDashboard(kMorning));
        QCOMPARE(f.bar()->count(), 1);
        QCOMPARE(list->ids(), QStringList{kRelease});
    }

    void itemWindow()
    {
        Fixture f;
        f.window->openItemWindow(kOverviewTask, kReviewsItem);
        auto windows = f.window->findChildren<ItemWindow *>();
        QCOMPARE(windows.size(), 1);
        QPointer<ItemWindow> window = windows.front();
        QVERIFY(window->isWindow());
        QVERIFY(window->isVisible());
        QCOMPARE(f.bar()->previewIndex(), -1);     // no task dashboard is opened

        // The window holds only the item.
        const auto frames = window->findChildren<CellFrame *>();
        QCOMPARE(frames.size(), 1);
        QVERIFY(frames.front()->ref());
        QCOMPARE(frames.front()->ref()->item, kReviewsItem);
        QVERIFY(window->windowTitle().contains(f.model.task(kOverviewTask)->title));

        // Opening it again raises the same window.
        f.window->openItemWindow(kOverviewTask, kReviewsItem);
        QCOMPARE(f.window->findChildren<ItemWindow *>().size(), 1);

        // Escape closes it.
        QTest::keyClick(window.data(), Qt::Key_Escape);
        QTRY_VERIFY(window.isNull());

        // It also closes when its item is gone.
        f.window->openItemWindow(kOverviewTask, kReviewsItem);
        window = f.window->findChildren<ItemWindow *>().front();
        QVERIFY(store::load(QStringLiteral(MIRANTE_FIXTURES_DIR "/no-such-folder"), f.model).empty());
        QTRY_VERIFY(window.isNull());
    }

    void focusItem()
    {
        Fixture f;
        f.window->show();
        f.window->activateWindow();
        if (!QTest::qWaitForWindowActive(f.window.get()))
            QSKIP("The window cannot be activated on this platform.");

        f.window->focusItem(kOverviewTask, kReviewsItem);
        QCOMPARE(f.bar()->previewIndex(), 2);
        QWidget *focused = QApplication::focusWidget();
        QVERIFY(focused);
        auto *frame = qobject_cast<CellFrame *>(focused);
        QVERIFY(frame);
        QVERIFY(frame->ref());
        QCOMPARE(frame->ref()->item, kReviewsItem);
    }
};

QTEST_MAIN(TestMainWindow)
#include "tst_main_window.moc"
