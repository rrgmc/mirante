// Tests MainWindow on the fixtures: the root tabs, the preview tab, pinning,
// new root dashboards, closing and reopening tabs, the warning bar, focusing an
// item, and views following the model.
#include "core/Model.h"
#include "core/Store.h"
#include "items/ItemTypeRegistry.h"
#include "layout/CellFrame.h"
#include "layout/LayoutView.h"
#include "ui/MainWindow.h"
#include "ui/RootDashboardsDialog.h"
#include "ui/RootTabBar.h"
#include "ui/Sidebar.h"
#include "ui/SidebarModel.h"

#include <QApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QPointer>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
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
    // A workspace is a data folder. Opening one loads it and remembers it on
    // this device, first in the recent list. A missing folder is reported and
    // dropped from the list.
    void workspaces()
    {
        // Keep the test's settings apart from the app's.
        QCoreApplication::setOrganizationName(QStringLiteral("MiranteTests"));
        QSettings().remove(QStringLiteral("workspace"));

        Fixture f;
        f.window->openSampleData();
        QVERIFY(f.window->workspace().isEmpty());
        QVERIFY(f.window->windowTitle().contains(QStringLiteral("Sample data")));
        // The sample data is compiled into the app only, so the test checks no tabs here.

        const QString folder = QDir(QStringLiteral(MIRANTE_FIXTURES_DIR)).absolutePath();
        f.window->openWorkspace(folder);
        QCOMPARE(f.window->workspace(), folder);
        QCOMPARE(f.window->recentWorkspaces(), QStringList{folder});
        QCOMPARE(QSettings().value(QStringLiteral("workspace/last")).toString(), folder);
        // The fixtures have a workspace.mirante naming them, which the title shows.
        QVERIFY(f.window->windowTitle().startsWith(QStringLiteral("Sample data")));
        QCOMPARE(f.bar()->count(), 2);
        QVERIFY(f.window->findChild<QWidget *>(QStringLiteral("warningBar"))->isHidden());

        // Opening it again keeps one entry; a missing folder is reported.
        f.window->openWorkspace(folder);
        QCOMPARE(f.window->recentWorkspaces(), QStringList{folder});
        f.window->openWorkspace(folder + QStringLiteral("/no-such-folder"));
        QCOMPARE(f.window->workspace(), folder);
        QVERIFY(!f.window->findChild<QWidget *>(QStringLiteral("warningBar"))->isHidden());

        // The path of workspace.mirante opens its folder.
        f.window->openSampleData();
        f.window->openWorkspace(folder + QStringLiteral("/workspace.mirante"));
        QCOMPARE(f.window->workspace(), folder);

        QSettings().remove(QStringLiteral("workspace"));
    }

    // A folder is a workspace only with a workspace.mirante in it. Without
    // one it is not opened; createWorkspace writes the file and opens it.
    void workspaceNeedsItsFile()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("MiranteTests"));
        QSettings().remove(QStringLiteral("workspace"));
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        Fixture f;
        f.window->openSampleData();
        QVERIFY(!store::isWorkspace(dir.path()));
        f.window->openWorkspace(dir.path());
        QVERIFY(f.window->workspace().isEmpty());
        QVERIFY(!f.window->findChild<QWidget *>(QStringLiteral("warningBar"))->isHidden());

        QVERIFY(f.window->createWorkspace(dir.path(), QStringLiteral("Fresh")));
        QVERIFY(store::isWorkspace(dir.path()));
        QCOMPARE(f.window->workspace(), QDir(dir.path()).absolutePath());
        QVERIFY(f.window->windowTitle().startsWith(QStringLiteral("Fresh")));
        QVERIFY(f.window->findChild<QWidget *>(QStringLiteral("warningBar"))->isHidden());

        // A folder that does not exist cannot be made one.
        QVERIFY(!f.window->createWorkspace(dir.filePath(QStringLiteral("missing")), QStringLiteral("X")));
        QSettings().remove(QStringLiteral("workspace"));
    }

    // A workspace's name lives in its workspace.mirante. Renaming writes it, and
    // the sidebar title and the window title show it.
    void renameWorkspace()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("MiranteTests"));
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QDirIterator files(QStringLiteral(MIRANTE_FIXTURES_DIR), QDir::Files, QDirIterator::Subdirectories);
        while (files.hasNext()) {
            const QString source = files.next();
            const QString relative = QDir(QStringLiteral(MIRANTE_FIXTURES_DIR)).relativeFilePath(source);
            QVERIFY(QDir(dir.path()).mkpath(QFileInfo(relative).path()));
            QVERIFY(QFile::copy(source, dir.filePath(relative)));
        }

        Fixture f;
        f.window->openWorkspace(dir.path());
        auto *title = f.window->findChild<QLabel *>(QStringLiteral("workspaceTitle"));
        QVERIFY(title);
        QCOMPARE(title->text(), QStringLiteral("Sample data"));

        QVERIFY(f.window->renameWorkspace(QStringLiteral("Work stuff")));
        QCOMPARE(title->text(), QStringLiteral("Work stuff"));
        QVERIFY(f.window->windowTitle().startsWith(QStringLiteral("Work stuff")));

        // Opening it again reads the name back.
        f.window->openSampleData();
        f.window->openWorkspace(dir.path());
        QCOMPARE(title->text(), QStringLiteral("Work stuff"));

        // The sample data cannot be renamed.
        f.window->openSampleData();
        QVERIFY(!f.window->renameWorkspace(QStringLiteral("Other")));
        QSettings().remove(QStringLiteral("workspace"));
    }

    // From the sidebar menu, a task dashboard or a single item opens in a new
    // root dashboard.
    void openInNewRootDashboard()
    {
        Fixture f;
        auto *sidebar = f.window->findChild<Sidebar *>();
        emit sidebar->openInNewRootDashboard(kFixLoginTask, kFixLoginDashboard);
        QCOMPARE(int(f.model.rootDashboards().size()), 3);
        const LayoutNode &taskTree = f.window->currentLayoutView()->tree();
        QVERIFY(taskTree.ref && taskTree.ref->isTask());
        QCOMPARE(taskTree.ref->dashboard, kFixLoginDashboard);

        emit sidebar->openItemInNewRootDashboard(kOverviewTask, kReviewsItem);
        QCOMPARE(int(f.model.rootDashboards().size()), 4);
        const LayoutNode &itemTree = f.window->currentLayoutView()->tree();
        QVERIFY(itemTree.ref && itemTree.ref->isItem());
        QCOMPARE(itemTree.ref->item, kReviewsItem);
        QCOMPARE(itemTree.ref->task, kOverviewTask);
    }

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

    // An item opens alone in a tab, like a task dashboard: in the preview tab,
    // with its type icon and a "Task / Item" title bar, and it can be pinned.
    void itemTab()
    {
        Fixture f;
        f.window->openItem(kOverviewTask, kReviewsItem);
        QCOMPARE(f.bar()->count(), 3);
        QCOMPARE(f.bar()->previewIndex(), 2);
        QVERIFY(!f.bar()->tabIcon(2).isNull());
        const Item *item = f.model.item(kOverviewTask, kReviewsItem);
        QCOMPARE(f.tabText(2), QStringLiteral("%1 / %2").arg(f.model.task(kOverviewTask)->title, item->name));

        // The page shows only the item, read-only, with no lock.
        LayoutView *view = f.window->currentLayoutView();
        QCOMPARE(view->tree().kind, LayoutNode::Kind::Cell);
        QVERIFY(view->tree().ref);
        QCOMPARE(view->tree().ref->item, kReviewsItem);
        QVERIFY(!view->editMode());
        QVERIFY(!view->parentWidget()->findChild<QToolButton *>(QStringLiteral("taskLockButton")));
        const auto frames = view->findChildren<CellFrame *>();
        QCOMPARE(frames.size(), 1);

        // Opening it again switches to its tab. A task dashboard replaces it in
        // the preview tab, unless it is pinned.
        f.window->openRootDashboard(kMorning);
        f.window->openItem(kOverviewTask, kReviewsItem);
        QCOMPARE(f.bar()->currentIndex(), 2);
        auto *pin = f.bar()->findChild<QToolButton *>(QStringLiteral("pinButton"));
        QVERIFY(pin);
        pin->click();
        QCOMPARE(f.bar()->previewIndex(), -1);
        f.window->openTaskPreview(kFixLoginTask);
        QCOMPARE(f.bar()->count(), 4);
        QCOMPARE(f.bar()->previewIndex(), 3);

        // The tab closes when its item is gone.
        QVERIFY(store::load(QStringLiteral(MIRANTE_FIXTURES_DIR "/no-such-folder"), f.model).empty());
        QCOMPARE(f.bar()->count(), 0);
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
