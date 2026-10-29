// Tests MainWindow on the fixtures: the root tabs, the preview tab, pinning,
// new root dashboards, closing and reopening tabs, the warning bar, focusing an
// item, views following the model, detached windows, and the web views of
// item cells.
#include "core/DeviceSettings.h"
#include "core/Model.h"
#include "core/Store.h"
#include "core/WebProfiles.h"
#include "items/ItemTypeRegistry.h"
#include "items/Placement.h"
#include "items/ViewPool.h"
#include "layout/CellFrame.h"
#include "layout/LayoutView.h"
#include "layout/TaskCell.h"
#include "ui/DashboardPage.h"
#include "ui/DetachedWindow.h"
#include "ui/EntityActions.h"
#include "ui/FocusView.h"
#include "ui/ImagePaste.h"
#include "ui/MainWindow.h"
#include "ui/RootDashboardsDialog.h"
#include "ui/RootTabBar.h"
#include "ui/Sidebar.h"
#include "ui/SidebarDelegate.h"
#include "ui/SidebarModel.h"
#include "ui/Shortcuts.h"
#include "ui/WebProfilesDialog.h"
#include "web/ProfileManager.h"
#include "web/WebPanel.h"

#include <mirante/ItemContext.h>
#include <mirante/ItemType.h>

#include <QApplication>
#include <QDir>
#include <QDirIterator>
#include <QEnterEvent>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QPointer>
#include <QSettings>
#include <QSignalSpy>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>
#include <QTimer>
#include <QTreeView>
#include <QWebEngineView>

#include <memory>

using namespace mirante;

namespace {

const QString kMorning = QStringLiteral("60139a76-dd45-50ea-890b-ae022026e421");
const QString kRelease = QStringLiteral("5247a9e5-ee54-5552-8831-0b2ff361afba");
const QString kFixLoginTask = QStringLiteral("760c8e20-8555-5a2c-bcbe-75e7fd08ca74");
const QString kFixLoginDashboard = QStringLiteral("e7c480bf-fc19-50f3-81e4-18fc51f27f6c");
const QString kOverviewTask = QStringLiteral("c34def7a-af34-507f-a44d-770bb60dbe95");
const QString kReviewsItem = QStringLiteral("191057b6-8355-5994-beae-62a500d35f8d");
// A web.page item of the Accessibility audit task.
const QString kAuditTask = QStringLiteral("2038cae6-f1d0-5380-a038-ecc1602c4c5a");
const QString kAuditPage = QStringLiteral("161e69b0-d18e-595b-8a31-08f457e40d3b");

// The page every test web item shows, in place of the fixture's address, so
// no test uses the network.
const QUrl kTestPage(QStringLiteral("data:text/html,<p>test page</p>"));

// A stand-in for the web module's web.page, which the ui tests do not link.
class TestWebPage : public ItemType
{
public:
    QString id() const override { return QStringLiteral("web.page"); }
    QString name() const override { return QStringLiteral("Web page"); }
    QIcon icon() const override { return {}; }
    Rendering rendering() const override { return Rendering::Web; }
    QWidget *createView(ItemContext &context, QWidget *parent) const override
    {
        return context.createWebView(kTestPage, parent);
    }
};

void registerTestWeb(ModuleRegistrar &registrar) { registrar.addItemType(std::make_unique<TestWebPage>()); }

// The registry has no modules, so every item has an unknown type.
struct Fixture;
QString overviewTitle(const Fixture &f);

// Copies the fixtures into a folder, so a test can open them as a workspace
// that is written to.
bool copyFixtures(const QString &target)
{
    QDirIterator files(QStringLiteral(MIRANTE_FIXTURES_DIR), QDir::Files, QDirIterator::Subdirectories);
    while (files.hasNext()) {
        const QString source = files.next();
        const QString relative = QDir(QStringLiteral(MIRANTE_FIXTURES_DIR)).relativeFilePath(source);
        if (!QDir(target).mkpath(QFileInfo(relative).path()) || !QFile::copy(source, QDir(target).filePath(relative)))
            return false;
    }
    return true;
}

// The window starts on the fixtures, loaded without the Store, so nothing is
// written until a test opens a workspace. The settings live in a temporary
// INI file. With web set, item cells get real views: web.page items show a
// data: page, in profiles of a temporary folder.
struct Fixture
{
    QTemporaryDir settingsDir;
    QSettings iniSettings{settingsDir.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat};
    DeviceSettings settings{&iniSettings};
    ItemTypeRegistry types;
    Model model;
    Store dataStore{model};
    QString profilesRoot;
    std::unique_ptr<ProfileManager> profiles;
    std::unique_ptr<ViewPool> pool;
    std::unique_ptr<MainWindow> window;

    explicit Fixture(bool web = false)
    {
        const auto problems = store::load(QStringLiteral(MIRANTE_FIXTURES_DIR), model);
        if (!problems.empty())
            qWarning() << "fixture problems:" << problems.size();
        if (web) {
            types.registerModule(QStringLiteral("web"), &registerTestWeb);
            QTemporaryDir root;
            root.setAutoRemove(false);
            profilesRoot = root.path();
            profiles = std::make_unique<ProfileManager>(profilesRoot);
            pool = std::make_unique<ViewPool>();
        }
        window = std::make_unique<MainWindow>(model, types, dataStore, settings, profiles.get(), pool.get());
    }

    ~Fixture()
    {
        // The pages go before their profiles. Chromium closes some profile
        // files a moment after the profile goes.
        window.reset();
        profiles.reset();
        if (!profilesRoot.isEmpty())
            (void)QTest::qWaitFor([this] { return QDir(profilesRoot).removeRecursively(); }, 15000);
    }

    QWidget *warningBar() const { return window->findChild<QWidget *>(QStringLiteral("warningBar")); }
    QString warningText() const
    {
        const auto labels = warningBar()->findChildren<QLabel *>();
        return labels.isEmpty() ? QString() : labels.front()->text();
    }

    RootTabBar *bar() const { return window->rootTabBar(); }
    QString tabText(int index) const { return bar()->tabText(index); }

    // The group of a task.
    QString groupOf(const QString &taskId) const
    {
        const Task *task = model.task(taskId);
        const Project *project = task ? model.project(task->projectId) : nullptr;
        return project ? project->groupId : QString();
    }
};

// The first item cell in a widget, or the one showing itemId.
CellFrame *itemCell(const QWidget *within, const QString &itemId = QString())
{
    for (CellFrame *frame : within->findChildren<CellFrame *>()) {
        const auto ref = frame->ref();
        if (ref && ref->isItem() && (itemId.isEmpty() || ref->item == itemId))
            return frame;
    }
    return nullptr;
}

// Opens a view's cell menu, and chooses the entry named choose if there is one.
void chooseInCellMenu(LayoutView &view, const NodePath &path, const QString &choose)
{
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, [choose] {
        auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
        if (!menu)
            return;
        for (QAction *action : menu->actions()) {
            if (action->text() == choose) {
                menu->setActiveAction(action);
                QTest::keyClick(menu, Qt::Key_Return);
                return;
            }
        }
        menu->close();
    });
    timer.start(0);
    emit view.cellMenuRequested(path, view.mapToGlobal(QPoint(10, 10)));
}

// Records the warnings Qt prints while it exists.
struct WarningLog
{
    static inline QStringList messages;
    QtMessageHandler previous = nullptr;

    WarningLog()
    {
        messages.clear();
        previous = qInstallMessageHandler([](QtMsgType type, const QMessageLogContext &, const QString &message) {
            if (type == QtWarningMsg || type == QtCriticalMsg)
                messages << message;
        });
    }
    ~WarningLog() { qInstallMessageHandler(previous); }
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

    void newDashboardOpensInPreview()
    {
        Fixture f;
        const QString review = QStringLiteral("ece7e850-6e0f-51ef-ad23-4ee948afdd66");
        emit f.window->entityActions()->dashboardCreated(kFixLoginTask, review);
        QCOMPARE(f.bar()->previewIndex(), 2);
        QCOMPARE(f.bar()->currentIndex(), 2);
        const LayoutNode *reviewTree = f.model.layout(DashboardRef{kFixLoginTask, review});
        QVERIFY(reviewTree);
        QVERIFY(layout::sameShape(f.window->currentLayoutView()->tree(), *reviewTree));
    }

    void otherTaskChangesKeepPreview()
    {
        Fixture f;
        f.window->openTaskPreview(kFixLoginTask);
        QCOMPARE(f.bar()->previewIndex(), 2);
        // The tooltip names the project, which the tab title leaves out.
        QCOMPARE(f.bar()->tabToolTip(2), QStringLiteral("backend / Fix login timeout / Work"));

        // A change to the task that leaves this dashboard's layout as it is,
        // such as a new title or a new dashboard, does not pin the preview.
        Task task = *f.model.task(kFixLoginTask);
        task.title = QStringLiteral("Login timeout");
        f.model.setTask(task);
        QTest::qWait(20);
        QCOMPARE(f.bar()->previewIndex(), 2);
        QCOMPARE(f.bar()->tabToolTip(2), QStringLiteral("backend / Login timeout / Work"));
    }

    // View > Full Screen uses the platform's key, and leaving full screen
    // brings back the window state from before.
    void fullScreen()
    {
        Fixture f;
        QAction *action = nullptr;
        for (QAction *candidate : f.window->findChildren<QAction *>()) {
            if (candidate->text() == QStringLiteral("&Full Screen"))
                action = candidate;
        }
        QVERIFY(action);
        QCOMPARE(action->shortcut(), QKeySequence(QKeySequence::FullScreen));
        QVERIFY(action->isCheckable());

        f.window->show();
        QVERIFY(QTest::qWaitForWindowExposed(f.window.get()));
        f.window->showMaximized();
        action->trigger();
        QTRY_VERIFY(f.window->windowState().testFlag(Qt::WindowFullScreen));
        QVERIFY(action->isChecked());
        // The menu bar hides, but the menus' shortcuts still work: the key
        // leaves full screen.
        QTRY_VERIFY(!f.window->menuBar()->isVisible());
        QVERIFY(f.window->actions().contains(action));
        QTest::keySequence(f.window.get(), action->shortcut());
        QTRY_VERIFY(!f.window->windowState().testFlag(Qt::WindowFullScreen));
        QVERIFY(f.window->windowState().testFlag(Qt::WindowMaximized));
        QVERIFY(!action->isChecked());
        QVERIFY(f.window->menuBar()->isVisible());
    }

    // The Focus tab has an icon, so it reads apart from dashboard tabs.
    void focusTabIcon()
    {
        Fixture f;
        f.window->openFocus();
        QVERIFY(!f.bar()->tabIcon(0).isNull());
        QVERIFY(f.bar()->tabIcon(1).isNull());     // Morning, a root dashboard
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
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(copyFixtures(dir.path()));

        Fixture f;
        f.window->openSampleData();
        QVERIFY(f.window->workspace().isEmpty());
        QVERIFY(f.window->windowTitle().contains(QStringLiteral("Sample data")));
        // The sample data is compiled into the app only, so the test checks no tabs here.

        const QString folder = QDir(dir.path()).absolutePath();
        f.window->openWorkspace(folder);
        QCOMPARE(f.window->workspace(), folder);
        QCOMPARE(f.window->recentWorkspaces(), QStringList{folder});
        QCOMPARE(f.settings.lastWorkspace(), folder);
        QCOMPARE(f.dataStore.dir(), folder);
        // The fixtures have a workspace.mirante naming them, which the title shows.
        QVERIFY(f.window->windowTitle().startsWith(QStringLiteral("Sample data")));
        QCOMPARE(f.bar()->count(), 2);
        QVERIFY(f.warningBar()->isHidden());

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
    }

    // A folder is a workspace only with a workspace.mirante in it. Without
    // one it is not opened; createWorkspace writes the file and opens it.
    void workspaceNeedsItsFile()
    {
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
    }

    // A workspace's name lives in its workspace.mirante. Renaming writes it, and
    // the sidebar title and the window title show it.
    void renameWorkspace()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(copyFixtures(dir.path()));

        Fixture f;
        f.window->openWorkspace(dir.path());
        auto *title = f.window->findChild<QLabel *>(QStringLiteral("workspaceTitle"));
        QVERIFY(title);
        QCOMPARE(title->text(), QStringLiteral("Sample data"));

        QVERIFY(f.window->renameWorkspace(QStringLiteral("Work stuff")));
        QCOMPARE(title->text(), QStringLiteral("Work stuff"));
        QVERIFY(f.window->windowTitle().startsWith(QStringLiteral("Work stuff")));

        // The Store writes the name into workspace.mirante.
        f.dataStore.flush();
        QFile file(dir.filePath(QStringLiteral("workspace.mirante")));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(QJsonDocument::fromJson(file.readAll()).object().value(QStringLiteral("name")).toString(),
                 QStringLiteral("Work stuff"));
        file.close();

        // Opening it again reads the name back.
        f.window->openSampleData();
        f.window->openWorkspace(dir.path());
        QCOMPARE(title->text(), QStringLiteral("Work stuff"));

        // The sample data cannot be renamed.
        f.window->openSampleData();
        QVERIFY(!f.window->renameWorkspace(QStringLiteral("Other")));
    }

    // An edit through EntityActions reaches the file on disk.
    void editReachesDisk()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(copyFixtures(dir.path()));

        Fixture f;
        f.window->openWorkspace(dir.path());
        QVERIFY(!f.dataStore.readOnly());
        QCOMPARE(f.model.task(kFixLoginTask)->state, TaskState::Active);
        f.window->entityActions()->setState(kFixLoginTask, TaskState::Done);
        f.dataStore.flush();

        QFile file(dir.filePath(QStringLiteral("tasks/%1.json").arg(kFixLoginTask)));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(QJsonDocument::fromJson(file.readAll()).object().value(QStringLiteral("state")).toString(),
                 QStringLiteral("done"));
    }

    // The open tabs of a workspace are saved when another one opens, and come
    // back when it opens again: root dashboards and Focus in order, and the
    // current tab. The preview tab is not kept.
    void openTabsRestored()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(copyFixtures(dir.path()));

        Fixture f;
        f.window->openWorkspace(dir.path());
        QCOMPARE(f.bar()->count(), 2);      // nothing saved yet: every root dashboard
        f.window->openFocus();
        f.window->openRootDashboard(kRelease);
        f.window->closeCurrentTab();
        f.window->openTaskPreview(kFixLoginTask);
        QCOMPARE(f.bar()->count(), 3);

        // The preview was current, so the first tab is current now.
        f.window->openSampleData();
        f.window->openWorkspace(dir.path());
        QCOMPARE(f.bar()->count(), 2);
        QCOMPARE(f.tabText(0), QStringLiteral("Focus"));
        QCOMPARE(f.tabText(1), QStringLiteral("Morning"));
        QCOMPARE(f.bar()->previewIndex(), -1);
        QCOMPARE(f.bar()->currentIndex(), 0);

        // Reloading keeps them, and so does a new window on the same settings.
        f.window->openRootDashboard(kMorning);
        f.window->reloadWorkspace();
        QCOMPARE(f.bar()->count(), 2);
        QCOMPARE(f.bar()->currentIndex(), 1);
        f.window->close();
        f.window.reset();
        f.dataStore.close();    // as at start: nothing is open yet
        f.window = std::make_unique<MainWindow>(f.model, f.types, f.dataStore, f.settings);
        f.window->openWorkspace(dir.path());
        QCOMPARE(f.bar()->count(), 2);
        QCOMPARE(f.tabText(0), QStringLiteral("Focus"));
        QCOMPARE(f.bar()->currentIndex(), 1);
    }

    // The Focus entry opens one Focus tab, at the start of the tab bar.
    void focusTab()
    {
        Fixture f;
        auto *sidebar = f.window->findChild<Sidebar *>();
        emit sidebar->focusRequested();
        QCOMPARE(f.bar()->count(), 3);
        QCOMPARE(f.tabText(0), QStringLiteral("Focus"));
        QCOMPARE(f.bar()->currentIndex(), 0);
        QVERIFY(!f.window->currentLayoutView());

        f.window->openRootDashboard(kRelease);
        emit sidebar->focusRequested();
        QCOMPARE(f.bar()->count(), 3);
        QCOMPARE(f.bar()->currentIndex(), 0);

        // A task opened from it goes to the preview tab.
        auto *focus = f.window->findChild<FocusView *>();
        QVERIFY(focus);
        emit focus->taskOpened(kFixLoginTask);
        QCOMPARE(f.bar()->count(), 4);
        QCOMPARE(f.bar()->previewIndex(), 3);
        QCOMPARE(f.tabText(3), QStringLiteral("Fix login timeout / Work"));

        // It closes like any other tab.
        emit f.bar()->tabCloseRequested(0);
        QCOMPARE(f.bar()->count(), 3);
        QVERIFY(!f.window->findChild<FocusView *>());
    }

    // The sample data is read-only, and the warning bar says so.
    void readOnlyNote()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(copyFixtures(dir.path()));

        Fixture f;
        QVERIFY(f.warningBar()->isHidden());
        f.window->openSampleData();
        QVERIFY(f.dataStore.readOnly());
        QVERIFY(!f.warningBar()->isHidden());
        QVERIFY(f.warningText().contains(QStringLiteral("not saved")));

        f.window->openWorkspace(dir.path());
        QVERIFY(f.warningBar()->isHidden());
    }

    // The + menu in the sidebar header makes a project in the selected group
    // and a task in the selected project, so each needs a selection.
    void newMenu()
    {
        Fixture f;
        auto *button = f.window->findChild<QToolButton *>(QStringLiteral("newEntityButton"));
        QVERIFY(button && button->menu());
        const auto actions = button->menu()->actions();
        QCOMPARE(actions.size(), 3);
        emit button->menu()->aboutToShow();
        QVERIFY(actions[0]->isEnabled());
        QVERIFY(!actions[1]->isEnabled());
        QVERIFY(!actions[2]->isEnabled());

        f.window->revealInSidebar(kFixLoginTask);
        emit button->menu()->aboutToShow();
        QVERIFY(actions[1]->isEnabled());
        QVERIFY(actions[2]->isEnabled());
    }

    // A pasted image that cannot be placed is explained in the status bar.
    // The registry has no type that takes pasted images.
    void pasteImageMessage()
    {
        Fixture f;
        QImage image(4, 4, QImage::Format_ARGB32);
        image.fill(Qt::red);
        f.window->pasteImage(image);
        QCOMPARE(f.window->statusBar()->currentMessage(), ImagePaste::message(ImagePaste::Result::NoImageType));
    }

    // A deleted task's tabs close.
    void removedTaskClosesItsTabs()
    {
        Fixture f;
        f.window->openTaskPreview(kFixLoginTask);
        QCOMPARE(f.bar()->count(), 3);
        f.model.removeTask(kFixLoginTask);
        QCOMPARE(f.bar()->count(), 2);
    }

    // A task moved to the trash, alone or with its project, closes its tabs
    // and does not open again until restored.
    void trashedTaskClosesItsTabs()
    {
        Fixture f;
        f.window->openTaskPreview(kFixLoginTask);
        QCOMPARE(f.bar()->count(), 3);
        Task task = *f.model.task(kFixLoginTask);
        task.trashedAt = QStringLiteral("2026-10-29T10:00:00Z");
        f.model.setTask(task);
        QCOMPARE(f.bar()->count(), 2);
        f.window->openTaskPreview(kFixLoginTask);
        QCOMPARE(f.bar()->count(), 2);

        task.trashedAt.clear();
        f.model.setTask(task);
        f.window->openTaskPreview(kFixLoginTask);
        QCOMPARE(f.bar()->count(), 3);
        Project project = *f.model.project(task.projectId);
        project.trashedAt = QStringLiteral("2026-10-29T10:00:00Z");
        f.model.setProject(project);
        QCOMPARE(f.bar()->count(), 2);
    }

    // The sidebar tints the row of what the hovered cell shows: the task of a
    // task cell, or the item of an item cell, also inside a task cell.
    void hoverTintsSidebarRow()
    {
        Fixture f;
        auto *sidebar = f.window->findChild<Sidebar *>();
        auto *delegate = qobject_cast<SidebarDelegate *>(sidebar->findChild<QTreeView *>()->itemDelegate());
        QVERIFY(delegate);
        TaskCell *taskCell = nullptr;
        for (TaskCell *cell : f.window->currentLayoutView()->findChildren<TaskCell *>()) {
            if (cell->taskId() == kFixLoginTask)
                taskCell = cell;
        }
        QVERIFY(taskCell);
        CellFrame *itemCell = taskCell->view()->findChild<CellFrame *>();
        QVERIFY(itemCell && itemCell->ref());
        const QString itemId = itemCell->ref()->item;

        QEnterEvent enter(QPointF(1, 1), QPointF(1, 1), QPointF(1, 1));
        QEvent leave(QEvent::Leave);
        QCoreApplication::sendEvent(taskCell, &enter);
        QCOMPARE(sidebar->hoverHighlight(), kFixLoginTask);
        QVERIFY(delegate->isHoverTinted(sidebar->model()->indexOf(kFixLoginTask)));

        // The task is collapsed, so its row stands in for the hidden item row.
        QCoreApplication::sendEvent(itemCell, &enter);
        QCOMPARE(sidebar->hoverHighlight(), itemId);
        QCOMPARE(sidebar->hoverRow(), kFixLoginTask);
        QVERIFY(delegate->isHoverTinted(sidebar->model()->indexOf(kFixLoginTask)));

        // Once the item row can be seen, the tint moves to it. With the task
        // open but its Items row closed, the task stands in again.
        auto *tree = sidebar->findChild<QTreeView *>();
        const QModelIndex itemIndex = sidebar->model()->indexOf(itemId);
        tree->expand(itemIndex.parent().parent());
        tree->expand(itemIndex.parent());
        QCOMPARE(sidebar->hoverRow(), itemId);
        QVERIFY(delegate->isHoverTinted(itemIndex));
        QVERIFY(!delegate->isHoverTinted(sidebar->model()->indexOf(kFixLoginTask)));
        tree->collapse(itemIndex.parent());
        QCOMPARE(sidebar->hoverRow(), kFixLoginTask);
        tree->expand(itemIndex.parent());

        // A collapsed project stands in for everything under it.
        tree->collapse(sidebar->model()->indexOf(f.model.task(kFixLoginTask)->projectId));
        QCOMPARE(sidebar->hoverRow(), f.model.task(kFixLoginTask)->projectId);

        QCoreApplication::sendEvent(itemCell, &leave);
        QCOMPARE(sidebar->hoverHighlight(), kFixLoginTask);
        QCoreApplication::sendEvent(taskCell, &leave);
        QVERIFY(sidebar->hoverHighlight().isEmpty());
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

    // Detaching a root dashboard tab moves its page into a window of its own.
    // The page keeps its LayoutView and its cells. The window's lock is the
    // dashboard's lock.
    void detachMovesPage()
    {
        Fixture f;
        LayoutView *view = f.window->currentLayoutView();
        QVERIFY(view);
        const std::vector<QWidget *> cells = view->visibleCells();
        QVERIFY(!cells.empty());
        QVERIFY(f.bar()->tabData(0).toBool());

        emit f.bar()->detachRequested(0);
        QCOMPARE(f.bar()->count(), 1);
        QCOMPARE(f.tabText(0), QStringLiteral("Release"));
        const auto windows = detachedWindows();
        QCOMPARE(windows.size(), 1);
        DetachedWindow *window = windows.front();
        QCOMPARE(window->dashboardId(), kMorning);
        QCOMPARE(window->windowTitle(), QStringLiteral("Morning"));
        QVERIFY(window->isVisible());
        QVERIFY(window->page());
        QCOMPARE(window->page()->view, view);
        QVERIFY(view->window() == window);
        QCOMPARE(view->visibleCells(), cells);

        // The window's lock unlocks the dashboard and turns on edit mode.
        auto *lock = window->findChild<QToolButton *>(QStringLiteral("lockButton"));
        QVERIFY(lock);
        QVERIFY(!view->editMode());
        lock->click();
        QVERIFY(view->editMode());
        QCOMPARE(lock->text(), QStringLiteral("Editing"));
        lock->click();
        QVERIFY(!view->editMode());

        // A rename changes its title.
        RootDashboard renamed = *f.model.rootDashboard(kMorning);
        renamed.name = QStringLiteral("Mornings");
        f.model.setRootDashboard(renamed);
        QCOMPARE(window->windowTitle(), QStringLiteral("Mornings"));

        // Only root dashboard tabs can be detached: not Focus, the preview or
        // item tabs.
        f.window->openFocus();
        f.window->openTaskPreview(kFixLoginTask);
        QCOMPARE(f.bar()->count(), 3);
        QVERIFY(!f.bar()->tabData(0).toBool());
        QVERIFY(f.bar()->tabData(1).toBool());
        QVERIFY(!f.bar()->tabData(2).toBool());
        emit f.bar()->detachRequested(0);
        emit f.bar()->detachRequested(2);
        QCOMPARE(f.bar()->count(), 3);
        QCOMPARE(detachedWindows().size(), 1);
    }

    // Closing the window, or "Put Back", returns the page to the end of the
    // tab bar, before the preview tab, and makes it current.
    void closingPutsBack()
    {
        Fixture f;
        LayoutView *view = f.window->currentLayoutView();
        emit f.bar()->detachRequested(0);
        QPointer<DetachedWindow> window = detachedWindows().value(0);
        QVERIFY(window);
        window->close();
        QCOMPARE(f.bar()->count(), 2);
        QCOMPARE(f.tabText(1), QStringLiteral("Morning"));
        QCOMPARE(f.bar()->currentIndex(), 1);
        QCOMPARE(f.window->currentLayoutView(), view);
        QVERIFY(view->window() == f.window.get());
        QTRY_VERIFY(!window);
        QVERIFY(!f.settings.detachedGeometry(kMorning).isEmpty());

        f.window->openTaskPreview(kFixLoginTask);
        f.window->openRootDashboard(kRelease);
        emit f.bar()->detachRequested(0);
        window = detachedWindows().value(0);
        QVERIFY(window);
        auto *putBack = window->findChild<QToolButton *>(QStringLiteral("putBackButton"));
        QVERIFY(putBack);
        putBack->click();
        QCOMPARE(f.bar()->count(), 3);
        QCOMPARE(f.tabText(1), QStringLiteral("Release"));
        QCOMPARE(f.bar()->previewIndex(), 2);
        QCOMPARE(f.bar()->currentIndex(), 1);
        QTRY_VERIFY(!window);
    }

    // Opening a detached root dashboard, from the sidebar, the switcher or
    // the dashboard list, shows its window instead of a tab.
    void openingDetachedRaisesWindow()
    {
        Fixture f;
        emit f.bar()->detachRequested(0);
        QCOMPARE(f.bar()->count(), 1);
        DetachedWindow *window = detachedWindows().value(0);
        QVERIFY(window);
        window->hide();
        f.window->openRootDashboard(kMorning);
        QCOMPARE(f.bar()->count(), 1);
        QCOMPARE(detachedWindows().size(), 1);
        QVERIFY(window->isVisible());
    }

    // Deleting a root dashboard closes its window.
    void removingClosesWindow()
    {
        Fixture f;
        emit f.bar()->detachRequested(0);
        QPointer<DetachedWindow> window = detachedWindows().value(0);
        QVERIFY(window);
        f.model.removeRootDashboard(kMorning);
        QVERIFY(!window);
        QCOMPARE(f.bar()->count(), 1);
        QCOMPARE(detachedWindows().size(), 0);
    }

    // The detached root dashboards are saved with the open tabs, and come
    // back in their windows at their stored geometry.
    void detachedRestored()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(copyFixtures(dir.path()));

        Fixture f;
        f.window->openWorkspace(dir.path());
        emit f.bar()->detachRequested(0);
        DetachedWindow *window = detachedWindows().value(0);
        QVERIFY(window);
        window->resize(640, 480);
        f.window->saveOpenTabs();
        const DeviceSettings::Tabs tabs = f.settings.openTabs(QDir(dir.path()).absolutePath());
        QCOMPARE(tabs.detached, QStringList{kMorning});
        QCOMPARE(tabs.ids, QStringList{kRelease});

        // Another workspace closes the window, which keeps its geometry.
        f.window->openSampleData();
        QCOMPARE(detachedWindows().size(), 0);
        QVERIFY(!f.settings.detachedGeometry(kMorning).isEmpty());

        f.window->openWorkspace(dir.path());
        QCOMPARE(f.bar()->count(), 1);
        QCOMPARE(f.tabText(0), QStringLiteral("Release"));
        window = detachedWindows().value(0);
        QVERIFY(window);
        QCOMPARE(window->dashboardId(), kMorning);
        QCOMPARE(window->size(), QSize(640, 480));

        // Reloading keeps it detached.
        f.window->reloadWorkspace();
        QCOMPARE(f.bar()->count(), 1);
        QCOMPARE(detachedWindows().size(), 1);

        // Closing the main window puts it back, after saving it as detached,
        // so a new window opens it detached again.
        f.window->close();
        QCOMPARE(f.bar()->count(), 2);
        QCOMPARE(detachedWindows().size(), 0);
        f.window.reset();
        f.dataStore.close();
        f.window = std::make_unique<MainWindow>(f.model, f.types, f.dataStore, f.settings);
        f.window->openWorkspace(dir.path());
        QCOMPARE(f.bar()->count(), 1);
        QCOMPARE(detachedWindows().size(), 1);
        QCOMPARE(detachedWindows().front()->size(), QSize(640, 480));
    }

    // A cell in a detached window is maximised within its own window's page.
    void maximiseInDetachedWindow()
    {
        Fixture f;
        f.window->show();
        emit f.bar()->detachRequested(0);
        DetachedWindow *window = detachedWindows().value(0);
        QVERIFY(window);
        CellFrame *cell = itemCell(window->page());
        QVERIFY(cell);

        f.window->shortcuts()->toggleMaximised(cell);
        QVERIFY(f.window->shortcuts()->isMaximised(cell));
        QCOMPARE(cell->window(), window);
        QWidget *layer = cell->parentWidget();
        QCOMPARE(layer->objectName(), QStringLiteral("maximiseLayer"));
        QCOMPARE(layer->parentWidget(), window->page());
        QCOMPARE(layer->geometry(), window->page()->rect());

        // Changing the main window's tab leaves it maximised.
        f.window->openTaskPreview(kFixLoginTask);
        QVERIFY(f.window->shortcuts()->isMaximised(cell));

        f.window->shortcuts()->toggleMaximised(cell);
        QVERIFY(!f.window->shortcuts()->isMaximised(cell));
        QCOMPARE(cell->window(), window);
        QVERIFY(window->page()->view->isAncestorOf(cell));

        // A cell of the main window is maximised over its content area.
        CellFrame *mainCell = itemCell(f.window->currentLayoutView());
        QVERIFY(mainCell);
        f.window->shortcuts()->toggleMaximised(mainCell);
        QCOMPARE(mainCell->parentWidget()->parentWidget(), f.window->contentArea());
        f.window->shortcuts()->toggleMaximised(mainCell);
        QVERIFY(f.window->currentLayoutView()->isAncestorOf(mainCell));
    }

    // The cell's maximise button and menu reach Shortcuts, and "Open Alone"
    // reaches openItem.
    void biggerViewCallbacks()
    {
        Fixture f;
        f.window->show();
        QVERIFY(QTest::qWaitForWindowExposed(f.window.get()));
        LayoutView *view = f.window->currentLayoutView();
        CellFrame *cell = itemCell(view);
        QVERIFY(cell);
        const QString itemId = cell->ref()->item;
        const auto path = view->pathOf(cell);
        QVERIFY(path);

        emit cell->maximiseRequested();
        QVERIFY(f.window->shortcuts()->isMaximised(cell));
        chooseInCellMenu(*view, *path, QStringLiteral("Restore"));
        QTRY_VERIFY(!f.window->shortcuts()->isMaximised(cell));
        chooseInCellMenu(*view, *path, QStringLiteral("Maximise"));
        QTRY_VERIFY(f.window->shortcuts()->isMaximised(cell));
        emit cell->maximiseRequested();
        QVERIFY(!f.window->shortcuts()->isMaximised(cell));

        chooseInCellMenu(*view, *path, QStringLiteral("Open Alone"));
        QTRY_COMPARE(f.bar()->previewIndex(), 2);
        QCOMPARE(f.bar()->currentIndex(), 2);
        const LayoutNode &tree = f.window->currentLayoutView()->tree();
        QVERIFY(tree.ref);
        QCOMPARE(tree.ref->item, itemId);
    }

    // Without profiles, item cells keep their placeholders. With them, an item
    // cell's body is a Placement.
    void itemCellsGetPlacements()
    {
        {
            Fixture f;
            CellFrame *cell = itemCell(f.window->currentLayoutView());
            QVERIFY(cell);
            QVERIFY(!qobject_cast<Placement *>(cell->body()));
        }
        Fixture f(true);
        CellFrame *cell = itemCell(f.window->currentLayoutView());
        QVERIFY(cell);
        auto *placement = qobject_cast<Placement *>(cell->body());
        QVERIFY(placement);
        QCOMPARE(placement->itemId(), cell->ref()->item);
    }

    // A web item's cell shows a web page, in its effective web profile.
    void webItemShowsPage()
    {
        Fixture f(true);
        f.window->show();
        QVERIFY(QTest::qWaitForWindowExposed(f.window.get()));
        f.window->openItem(kAuditTask, kAuditPage);
        CellFrame *cell = itemCell(f.window->currentLayoutView(), kAuditPage);
        QVERIFY(cell);
        auto *placement = qobject_cast<Placement *>(cell->body());
        QVERIFY(placement);
        QTRY_VERIFY(qobject_cast<WebPanel *>(placement->view()));
        auto *panel = qobject_cast<WebPanel *>(placement->view());
        QCOMPARE(panel->view()->url(), kTestPage);
        const QString profileId = webprofiles::effective(f.model, kAuditTask, kAuditPage);
        QCOMPARE(panel->view()->page()->profile(), f.profiles->profile(profileId));
        QCOMPARE(placement->headerWidget(), panel->toolbar());
    }

    // Deleting a web profile in the web profiles dialog deletes its logins,
    // after the pages that used it: here a preview tab and a detached window.
    // Those pages are made again in the profile they fall back to. Deleting a
    // group deletes no profile.
    void deletingProfileRemovesItsLogins()
    {
        WarningLog warnings;
        {
            Fixture f(true);
            f.window->show();
            QVERIFY(QTest::qWaitForWindowExposed(f.window.get()));

            // The task's group uses its own profile.
            WebProfile audit;
            audit.id = QStringLiteral("audit-profile");
            audit.name = QStringLiteral("Audit");
            std::vector<WebProfile> profiles = f.model.webProfiles();
            profiles.push_back(audit);
            f.model.setWebProfiles(profiles);
            Group group = *f.model.group(f.groupOf(kAuditTask));
            group.webProfile = audit.id;
            f.model.setGroup(group);

            emit f.window->findChild<Sidebar *>()->openItemInNewRootDashboard(kAuditTask, kAuditPage);
            emit f.bar()->detachRequested(f.bar()->currentIndex());
            DetachedWindow *window = detachedWindows().value(0);
            QVERIFY(window);
            f.window->openItem(kAuditTask, kAuditPage);
            CellFrame *detachedCell = itemCell(window->page(), kAuditPage);
            CellFrame *previewCell = itemCell(f.window->currentLayoutView(), kAuditPage);
            QVERIFY(detachedCell && previewCell);
            auto *detachedPlacement = qobject_cast<Placement *>(detachedCell->body());
            auto *previewPlacement = qobject_cast<Placement *>(previewCell->body());
            QVERIFY(detachedPlacement && previewPlacement);
            QTRY_VERIFY(detachedPlacement->view() && previewPlacement->view());
            const QPointer<QWidget> detachedView = detachedPlacement->view();
            const QPointer<QWidget> previewView = previewPlacement->view();
            QCOMPARE(qobject_cast<WebPanel *>(previewView.data())->view()->page()->profile(),
                     f.profiles->profile(audit.id));

            const QString folder = f.profiles->storagePath(ProfileManager::storageName(audit.id));
            QTRY_VERIFY(QFileInfo::exists(folder));

            f.window->entityActions()->setConfirm([](const QString &) { return true; });
            f.window->entityActions()->setEditProfiles([&audit](WebProfilesDialog &dialog) {
                dialog.select(audit.id);
                dialog.deleteSelected();
            });
            f.window->entityActions()->editWebProfiles();
            QVERIFY(!f.model.webProfile(audit.id));
            QTRY_VERIFY(!previewView);
            QTRY_VERIFY(!detachedView);
            QTRY_VERIFY_WITH_TIMEOUT(!QFileInfo::exists(folder), 15000);

            // The new pages use Default.
            auto *panel = qobject_cast<WebPanel *>(previewPlacement->view());
            QVERIFY(panel);
            QCOMPARE(panel->view()->page()->profile(), f.profiles->profile(kDefaultWebProfile));

            // Deleting the group leaves Default's logins alone.
            const QString defaultFolder = f.profiles->storagePath(ProfileManager::storageName(kDefaultWebProfile));
            QVERIFY(QFileInfo::exists(defaultFolder));
            f.window->entityActions()->removeGroup(group.id);
            QVERIFY(!f.model.group(group.id));
            QTest::qWait(100);
            QVERIFY(QFileInfo::exists(defaultFolder));
        }
        for (const QString &message : std::as_const(WarningLog::messages))
            QVERIFY2(!message.contains(QStringLiteral("profile"), Qt::CaseInsensitive), qPrintable(message));
    }

private:
    // The visible detached windows, ignoring those already put back.
    static QList<DetachedWindow *> detachedWindows()
    {
        QList<DetachedWindow *> windows;
        for (QWidget *widget : QApplication::topLevelWidgets()) {
            auto *window = qobject_cast<DetachedWindow *>(widget);
            if (window && window->page())
                windows << window;
        }
        return windows;
    }
};

int main(int argc, char *argv[])
{
    // Qt WebEngine needs shared OpenGL contexts, set before the application.
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    // Without a display, Chromium's GPU process can fail a check when a view
    // goes away. Pages render in software instead.
    if (qEnvironmentVariableIsEmpty("QTWEBENGINE_CHROMIUM_FLAGS"))
        qputenv("QTWEBENGINE_CHROMIUM_FLAGS", "--disable-gpu");
    QApplication app(argc, argv);
    TestMainWindow test;
    return QTest::qExec(&test, argc, argv);
}
#include "tst_main_window.moc"
