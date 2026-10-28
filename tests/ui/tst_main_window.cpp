// Tests MainWindow on the fixtures: the root tabs, the preview tab, pinning,
// new root dashboards, closing and reopening tabs, the warning bar, focusing an
// item, and views following the model.
#include "core/Model.h"
#include "core/Store.h"
#include "items/ItemTypeRegistry.h"
#include "layout/CellFrame.h"
#include "layout/LayoutView.h"
#include "ui/MainWindow.h"
#include "ui/RootTabBar.h"
#include "ui/Sidebar.h"

#include <QApplication>
#include <QLabel>
#include <QSignalSpy>
#include <QTest>
#include <QToolButton>

#include <memory>

using namespace mirante;

namespace {

const QString kMorning = QStringLiteral("60139a76-dd45-50ea-890b-ae022026e421");
const QString kRelease = QStringLiteral("5247a9e5-ee54-5552-8831-0b2ff361afba");
const QString kFixLoginTask = QStringLiteral("760c8e20-8555-5a2c-bcbe-75e7fd08ca74");
const QString kFixLoginDashboard = QStringLiteral("e7c480bf-fc19-50f3-81e4-18fc51f27f6c");
const QString kDefaultTask = QStringLiteral("c34def7a-af34-507f-a44d-770bb60dbe95");
const QString kReviewsItem = QStringLiteral("191057b6-8355-5994-beae-62a500d35f8d");

// The registry has no modules, so every item has an unknown type.
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
        QCOMPARE(f.tabText(2), QStringLiteral("Fix login timeout"));
        QVERIFY(f.bar()->findChild<QToolButton *>(QStringLiteral("pinButton")));
        f.window->show();
        QVERIFY(QTest::qWaitForWindowExposed(f.window.get()));
        QTRY_VERIFY(f.bar()->height() > 0);
        QVERIFY(!f.bar()->grab().isNull());     // paints the italic tab

        const LayoutNode *expected = f.model.layout(DashboardRef{kFixLoginTask, kFixLoginDashboard});
        QVERIFY(expected);
        QVERIFY(layout::sameShape(f.window->currentLayoutView()->tree(), *expected));

        // A second preview replaces the first.
        f.window->openTaskPreview(kDefaultTask);
        QCOMPARE(f.bar()->count(), 3);
        QCOMPARE(f.bar()->previewIndex(), 2);
        QCOMPARE(f.tabText(2), f.model.task(kDefaultTask)->title);

        // Closing the preview removes its tab.
        f.window->closeCurrentTab();
        QCOMPARE(f.bar()->count(), 2);
        QCOMPARE(f.bar()->previewIndex(), -1);
    }

    void pinTask()
    {
        Fixture f;
        f.window->openTaskPreview(kFixLoginTask);
        f.window->pinTask(kFixLoginTask);

        const auto dashboards = f.model.rootDashboards();
        QCOMPARE(int(dashboards.size()), 3);
        QCOMPARE(f.bar()->count(), 3);
        QCOMPARE(f.bar()->previewIndex(), -1);
        QCOMPARE(f.bar()->currentIndex(), 0);
        QCOMPARE(f.tabText(0), QStringLiteral("Fix login timeout"));

        const LayoutNode &tree = f.window->currentLayoutView()->tree();
        QCOMPARE(tree.kind, LayoutNode::Kind::Cell);
        QVERIFY(tree.ref && tree.ref->isTask());
        QCOMPARE(tree.ref->task, kFixLoginTask);
        QCOMPARE(tree.ref->dashboard, kFixLoginDashboard);

        // A preview of a pinned task switches to its tab.
        f.window->openRootDashboard(kRelease);
        QCOMPARE(f.bar()->currentIndex(), 2);
        f.window->openTaskPreview(kFixLoginTask);
        QCOMPARE(f.bar()->currentIndex(), 0);
        QCOMPARE(f.bar()->count(), 3);
        QCOMPARE(f.bar()->previewIndex(), -1);

        // Pinning it again makes no second dashboard.
        f.window->pinTask(kFixLoginTask);
        QCOMPARE(int(f.model.rootDashboards().size()), 3);
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
        QCOMPARE(int(f.model.rootDashboards().size()), 3);
        QCOMPARE(f.tabText(f.bar()->currentIndex()), QStringLiteral("Fix login timeout"));
    }

    void pinButton()
    {
        Fixture f;
        f.window->openTaskPreview(kDefaultTask);
        auto *pin = f.bar()->findChild<QToolButton *>(QStringLiteral("pinButton"));
        QVERIFY(pin);
        QSignalSpy spy(f.bar(), &RootTabBar::pinRequested);
        pin->click();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 2);
        QCOMPARE(f.bar()->previewIndex(), -1);
        QCOMPARE(f.bar()->count(), 3);
        QCOMPARE(f.tabText(0), f.model.task(kDefaultTask)->title);
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

    void focusItem()
    {
        Fixture f;
        f.window->show();
        f.window->activateWindow();
        if (!QTest::qWaitForWindowActive(f.window.get()))
            QSKIP("The window cannot be activated on this platform.");

        f.window->focusItem(kDefaultTask, kReviewsItem);
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
