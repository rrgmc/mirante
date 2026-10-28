// Tests LayoutEditor against the fixtures: the cell and tab commands, changing
// a reference, and each drop rule. Also tests the drag MIME data and the drop
// zones of DropOverlay on a shown LayoutView.
#include "core/Model.h"
#include "core/Store.h"
#include "items/ItemTypeRegistry.h"
#include "layout/CellFactory.h"
#include "layout/DragMime.h"
#include "layout/DropOverlay.h"
#include "layout/DropOverlayTarget.h"
#include "layout/LayoutEditor.h"
#include "layout/LayoutServices.h"
#include "layout/LayoutView.h"
#include "layout/TaskCell.h"

#include <QApplication>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QLabel>
#include <QMimeData>
#include <QSignalSpy>
#include <QTabBar>
#include <QTabWidget>
#include <QTest>

#include <memory>

using namespace mirante;

namespace {

// Task "Fix login timeout", with the dashboards "Work" and "Review".
const QString loginTask = QStringLiteral("760c8e20-8555-5a2c-bcbe-75e7fd08ca74");
const DashboardRef work{loginTask, QStringLiteral("e7c480bf-fc19-50f3-81e4-18fc51f27f6c")};
const DashboardRef review{loginTask, QStringLiteral("ece7e850-6e0f-51ef-ad23-4ee948afdd66")};
const QString claudeItem = QStringLiteral("3b610620-18e9-51b3-b413-51a7d696b733");     // "Claude: login fix"
const QString issuePageItem = QStringLiteral("81b47eda-8e42-55bf-aa48-da87340f6fd9");  // "ACME-412 (page)"
const QString notesItem = QStringLiteral("11039501-ba22-5677-86da-ea27842bf0b1");      // "Notes"

// Task "Add rate limiting", with the dashboard "Main".
const QString rateTask = QStringLiteral("d71bd575-6d1b-5088-9da0-7d2cebf0d59e");
const DashboardRef rateMain{rateTask, QStringLiteral("4eeaa216-cc9a-5055-bca2-5d504578d4e1")};
const QString rateIssueItem = QStringLiteral("e1fe6fb5-34ad-5838-9e03-bec6ea410351");
const QString rateTimeItem = QStringLiteral("627898f1-6342-5d8c-ac0e-ba08fa35b24c");

// A task with one dashboard.
const QString flakyTask = QStringLiteral("02fc9a52-653c-562f-a3a2-6d5a4ca98623");

// Root dashboards.
const DashboardRef morning{QString(), QStringLiteral("60139a76-dd45-50ea-890b-ae022026e421")};
const DashboardRef release{QString(), QStringLiteral("5247a9e5-ee54-5552-8831-0b2ff361afba")};

using Kind = LayoutNode::Kind;
using CellCommand = LayoutEditor::CellCommand;
using TabCommand = LayoutEditor::TabCommand;

CellRef itemRef(const QString &item, const QString &task = QString()) { return CellRef{item, task, QString()}; }
CellRef taskRef(const QString &task, const QString &dashboard = QString())
{
    return CellRef{QString(), task, dashboard};
}

DropTarget edge(const NodePath &path, Edge side)
{
    DropTarget target{path, {}};
    target.zone.kind = DropZone::Kind::Edge;
    target.zone.edge = side;
    return target;
}

DropTarget centre(const NodePath &path) { return DropTarget{path, {}}; }

DropTarget tabBar(const NodePath &path, int index)
{
    DropTarget target{path, {}};
    target.zone.kind = DropZone::Kind::TabBar;
    target.zone.tabIndex = index;
    return target;
}

DragPayload fromSidebar(const CellRef &ref) { return DragPayload{ref, std::nullopt, std::nullopt}; }

DragPayload moved(const CellRef &ref, const DashboardRef &source, const NodePath &path)
{
    return DragPayload{ref, source, path};
}

// Makes a TaskCell for a task reference and a label for anything else.
class FakeFactory : public CellFactory
{
public:
    QWidget *createCell(const std::optional<CellRef> &ref, bool /*removed*/, QWidget *parent) override
    {
        if (ref && ref->isTask())
            return new TaskCell(services, rootDashboard, *ref, parent);
        return new QLabel(ref ? ref->item : QStringLiteral("empty"), parent);
    }

    bool canReparent(const std::optional<CellRef> & /*ref*/) const override { return true; }

    LayoutServices services;
    DashboardRef rootDashboard;
};

} // namespace

class TestLayoutEditor : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<Model> model;
    std::unique_ptr<ItemTypeRegistry> types;
    std::unique_ptr<LayoutEditor> editor;

    const LayoutNode &tree(const DashboardRef &dashboard) const { return *model->layout(dashboard); }
    const LayoutNode &node(const DashboardRef &dashboard, const NodePath &path) const
    {
        const LayoutNode *found = layout::nodeAt(tree(dashboard), path);
        Q_ASSERT(found);
        return *found;
    }
    std::optional<CellRef> refAt(const DashboardRef &dashboard, const NodePath &path) const
    {
        return node(dashboard, path).ref;
    }
    QJsonObject json(const DashboardRef &dashboard) const { return layoutToJson(tree(dashboard)); }

private slots:
    void init()
    {
        model = std::make_unique<Model>();
        types = std::make_unique<ItemTypeRegistry>();
        QVERIFY(store::load(QStringLiteral(MIRANTE_FIXTURES_DIR), *model).empty());
        editor = std::make_unique<LayoutEditor>(*model, *types);

        // Work: claude | tabs[split(issue, mr) "Issue", pipeline "Pipeline", notes "Notes"]
        QCOMPARE(tree(work).kind, Kind::Split);
        QCOMPARE(tree(work).children.size(), std::size_t(2));
        QCOMPARE(refAt(work, {0}), itemRef(claudeItem));
        QCOMPARE(node(work, {1}).kind, Kind::Tabs);
        QCOMPARE(node(work, {1}).titles,
                 (std::vector<QString>{QStringLiteral("Issue"), QStringLiteral("Pipeline"), QStringLiteral("Notes")}));
    }

    void cleanup()
    {
        editor.reset();
        types.reset();
        model.reset();
    }

    // Cell commands

    void splitRightAddsAnEmptyCellOnTheRight()
    {
        editor->cellCommand(work, {0}, CellCommand::SplitRight);
        // The new split merges into the root split of the same direction.
        QCOMPARE(tree(work).children.size(), std::size_t(3));
        QCOMPARE(refAt(work, {0}), itemRef(claudeItem));
        QCOMPARE(node(work, {1}).kind, Kind::Cell);
        QVERIFY(!refAt(work, {1}));
        QCOMPARE(node(work, {2}).kind, Kind::Tabs);
    }

    void splitDownAddsAnEmptyCellBelow()
    {
        editor->cellCommand(work, {0}, CellCommand::SplitDown);
        const LayoutNode &split = node(work, {0});
        QCOMPARE(split.kind, Kind::Split);
        QCOMPARE(split.dir, Qt::Vertical);
        QCOMPARE(refAt(work, {0, 0}), itemRef(claudeItem));
        QVERIFY(!refAt(work, {0, 1}));
    }

    void wrapInTabsTitlesTheTabAfterTheItem()
    {
        editor->cellCommand(work, {0}, CellCommand::WrapInTabs);
        const LayoutNode &tabs = node(work, {0});
        QCOMPARE(tabs.kind, Kind::Tabs);
        QCOMPARE(tabs.titles, std::vector<QString>{QStringLiteral("Claude: login fix")});
        QCOMPARE(refAt(work, {0, 0}), itemRef(claudeItem));
    }

    void addTabWrapsACellAndAddsAnEmptyTab()
    {
        editor->cellCommand(work, {0}, CellCommand::AddTab);
        const LayoutNode &tabs = node(work, {0});
        QCOMPARE(tabs.kind, Kind::Tabs);
        QCOMPARE(tabs.titles, (std::vector<QString>{QStringLiteral("Claude: login fix"), QStringLiteral("New tab")}));
        QCOMPARE(tabs.current, 1);
        QVERIFY(!refAt(work, {0, 1}));
    }

    void addTabInATabGroupInsertsAfterTheCell()
    {
        editor->cellCommand(work, {1, 1}, CellCommand::AddTab);
        QCOMPARE(node(work, {1}).titles,
                 (std::vector<QString>{QStringLiteral("Issue"), QStringLiteral("Pipeline"), QStringLiteral("New tab"),
                                       QStringLiteral("Notes")}));
        QVERIFY(!refAt(work, {1, 2}));
    }

    void clearCellEmptiesIt()
    {
        editor->cellCommand(work, {0}, CellCommand::ClearCell);
        QCOMPARE(node(work, {0}).kind, Kind::Cell);
        QVERIFY(!refAt(work, {0}));
        QVERIFY(!node(work, {0}).removed);
    }

    void removeCellRemovesTheNode()
    {
        editor->cellCommand(work, {0}, CellCommand::RemoveCell);
        // The split is left with one child, which replaces it.
        QCOMPARE(tree(work).kind, Kind::Tabs);
        QCOMPARE(tree(work).children.size(), std::size_t(3));
    }

    void commandOnAMissingPathDoesNothing()
    {
        const QJsonObject before = json(work);
        editor->cellCommand(work, {5, 2}, CellCommand::RemoveCell);
        QCOMPARE(json(work), before);
    }

    // Changing the reference

    void changeItemInATaskDashboard()
    {
        editor->changeItem(work, {0}, itemRef(notesItem));
        QCOMPARE(refAt(work, {0}), itemRef(notesItem));
    }

    void changeItemRefusesAnotherTasksItem()
    {
        editor->changeItem(work, {0}, itemRef(rateIssueItem));
        QCOMPARE(refAt(work, {0}), itemRef(claudeItem));
        editor->changeItem(work, {0}, taskRef(flakyTask));
        QCOMPARE(refAt(work, {0}), itemRef(claudeItem));
    }

    void changeItemInARootDashboardFillsInTheReference()
    {
        // A task gets its default dashboard, and an item its task.
        editor->changeItem(morning, {0}, taskRef(flakyTask));
        QCOMPARE(refAt(morning, {0}), taskRef(flakyTask, model->task(flakyTask)->defaultDashboardId));
        editor->changeItem(morning, {0}, itemRef(rateIssueItem));
        QCOMPARE(refAt(morning, {0}), itemRef(rateIssueItem, rateTask));
    }

    // Tab commands

    void closeTabRemovesIt()
    {
        editor->tabCommand(work, {1}, 2, TabCommand::Close);
        QCOMPARE(node(work, {1}).titles, (std::vector<QString>{QStringLiteral("Issue"), QStringLiteral("Pipeline")}));
    }

    void moveTabLeftAndRight()
    {
        editor->tabCommand(work, {1}, 1, TabCommand::MoveLeft);
        QCOMPARE(node(work, {1}).titles,
                 (std::vector<QString>{QStringLiteral("Pipeline"), QStringLiteral("Issue"), QStringLiteral("Notes")}));
        editor->tabCommand(work, {1}, 1, TabCommand::MoveRight);
        QCOMPARE(node(work, {1}).titles,
                 (std::vector<QString>{QStringLiteral("Pipeline"), QStringLiteral("Notes"), QStringLiteral("Issue")}));

        // Past either end nothing changes.
        const QJsonObject before = json(work);
        editor->tabCommand(work, {1}, 0, TabCommand::MoveLeft);
        editor->tabCommand(work, {1}, 2, TabCommand::MoveRight);
        QCOMPARE(json(work), before);
    }

    void toggleCollapseSingle()
    {
        QVERIFY(!node(work, {1}).collapseSingle);
        editor->tabCommand(work, {1}, 0, TabCommand::ToggleCollapseSingle);
        QVERIFY(node(work, {1}).collapseSingle);
        editor->tabCommand(work, {1}, 0, TabCommand::ToggleCollapseSingle);
        QVERIFY(!node(work, {1}).collapseSingle);
    }

    void renameTab()
    {
        editor->renameTab(work, {1}, 1, QStringLiteral("CI"));
        QCOMPARE(node(work, {1}).titles,
                 (std::vector<QString>{QStringLiteral("Issue"), QStringLiteral("CI"), QStringLiteral("Notes")}));
    }

    // Drops from the sidebar

    void ownItemOnTheCentreOfAFilledCellAddsATab()
    {
        QVERIFY(editor->drop(work, centre({0}), fromSidebar(itemRef(issuePageItem, loginTask))));
        const LayoutNode &tabs = node(work, {0});
        QCOMPARE(tabs.kind, Kind::Tabs);
        QCOMPARE(tabs.titles,
                 (std::vector<QString>{QStringLiteral("Claude: login fix"), QStringLiteral("ACME-412 (page)")}));
        // A task dashboard stores the item alone.
        QCOMPARE(refAt(work, {0, 1}), itemRef(issuePageItem));
    }

    void itemOnTheCentreOfAnEmptyCellFillsIt()
    {
        editor->cellCommand(work, {0}, CellCommand::ClearCell);
        QVERIFY(editor->drop(work, centre({0}), fromSidebar(itemRef(issuePageItem))));
        QCOMPARE(node(work, {0}).kind, Kind::Cell);
        QCOMPARE(refAt(work, {0}), itemRef(issuePageItem));
    }

    void itemOnAnEdgeSplits()
    {
        QVERIFY(editor->drop(work, edge({0}, Edge::Left), fromSidebar(itemRef(issuePageItem))));
        QCOMPARE(tree(work).children.size(), std::size_t(3));
        QCOMPARE(refAt(work, {0}), itemRef(issuePageItem));
        QCOMPARE(refAt(work, {1}), itemRef(claudeItem));

        QVERIFY(editor->drop(work, edge({1}, Edge::Bottom), fromSidebar(itemRef(notesItem))));
        QCOMPARE(node(work, {1}).dir, Qt::Vertical);
        QCOMPARE(refAt(work, {1, 1}), itemRef(notesItem));
    }

    void itemOnATabBarInsertsATab()
    {
        QVERIFY(editor->drop(work, tabBar({1}, 1), fromSidebar(itemRef(issuePageItem))));
        QCOMPARE(node(work, {1}).titles,
                 (std::vector<QString>{QStringLiteral("Issue"), QStringLiteral("ACME-412 (page)"),
                                       QStringLiteral("Pipeline"), QStringLiteral("Notes")}));
        QCOMPARE(refAt(work, {1, 1}), itemRef(issuePageItem));
        QCOMPARE(node(work, {1}).current, 1);
    }

    void taskDashboardRefusesForeignItemsAndTasks()
    {
        const QJsonObject before = json(work);
        QVERIFY(!editor->drop(work, centre({0}), fromSidebar(itemRef(rateIssueItem, rateTask))));
        QVERIFY(!editor->drop(work, centre({0}), fromSidebar(itemRef(rateIssueItem))));
        QVERIFY(!editor->drop(work, edge({0}, Edge::Left), fromSidebar(taskRef(flakyTask))));
        QCOMPARE(json(work), before);
    }

    void taskFromTheSidebarIntoARootDashboard()
    {
        QVERIFY(editor->drop(morning, edge({0}, Edge::Right), fromSidebar(taskRef(flakyTask))));
        QCOMPARE(tree(morning).children.size(), std::size_t(3));
        QCOMPARE(refAt(morning, {1}), taskRef(flakyTask, model->task(flakyTask)->defaultDashboardId));
    }

    void itemFromTheSidebarIntoARootDashboardGetsItsTask()
    {
        QVERIFY(editor->drop(morning, tabBar({1}, 3), fromSidebar(itemRef(rateIssueItem))));
        QCOMPARE(refAt(morning, {1, 3}), itemRef(rateIssueItem, rateTask));
        QCOMPARE(node(morning, {1}).titles.back(), QStringLiteral("ACME-398"));
    }

    // Moves

    void moveWithinOneDashboard()
    {
        // Claude to the end of the tab group: the split collapses into the tabs.
        QVERIFY(editor->drop(work, tabBar({1}, 3), moved(itemRef(claudeItem), work, {0})));
        QCOMPARE(tree(work).kind, Kind::Tabs);
        QCOMPARE(tree(work).titles,
                 (std::vector<QString>{QStringLiteral("Issue"), QStringLiteral("Pipeline"), QStringLiteral("Notes"),
                                       QStringLiteral("Claude: login fix")}));
        QCOMPARE(refAt(work, {3}), itemRef(claudeItem));
    }

    void moveWithinOneDashboardToAnEdge()
    {
        // Notes, out of the tab group, below Claude.
        QVERIFY(editor->drop(work, edge({0}, Edge::Bottom), moved(itemRef(notesItem), work, {1, 2})));
        QCOMPARE(refAt(work, {0, 0}), itemRef(claudeItem));
        QCOMPARE(refAt(work, {0, 1}), itemRef(notesItem));
        QCOMPARE(node(work, {1}).children.size(), std::size_t(2));
    }

    void moveOntoItselfIsRefused()
    {
        const QJsonObject before = json(work);
        QVERIFY(!editor->drop(work, edge({0}, Edge::Left), moved(itemRef(claudeItem), work, {0})));
        QCOMPARE(json(work), before);
    }

    void moveAcrossDashboards()
    {
        QVERIFY(editor->drop(review, edge({0}, Edge::Right), moved(itemRef(claudeItem), work, {0})));
        // Gone from Work, whose root split collapses to the tab group.
        QCOMPARE(tree(work).kind, Kind::Tabs);
        QCOMPARE(tree(review).children.size(), std::size_t(3));
        QCOMPARE(refAt(review, {1}), itemRef(claudeItem));
    }

    void moveAcrossDashboardsKeepsTheTabTitle()
    {
        QVERIFY(editor->drop(review, tabBar({0}, 0), moved(itemRef(notesItem), work, {1, 2})));
        QCOMPARE(node(review, {0}).titles.front(), QStringLiteral("Notes"));
        QCOMPARE(node(work, {1}).children.size(), std::size_t(2));
    }

    void moveIntoATaskDashboardOfAnotherTaskIsRefused()
    {
        const QJsonObject workBefore = json(work);
        const QJsonObject mainBefore = json(rateMain);
        QVERIFY(!editor->drop(rateMain, centre({0}), moved(itemRef(claudeItem), work, {0})));
        QCOMPARE(json(work), workBefore);
        QCOMPARE(json(rateMain), mainBefore);
    }

    void moveFromARootDashboardIntoTheItemsTask()
    {
        // Release holds "Time: rate limiting" at {1, 1}, an item of the rate task.
        QCOMPARE(refAt(release, {1, 1}), itemRef(rateTimeItem, rateTask));
        QVERIFY(editor->drop(rateMain, edge({0}, Edge::Bottom),
                             moved(itemRef(rateTimeItem, rateTask), release, {1, 1})));
        QCOMPARE(refAt(rateMain, {0, 1}), itemRef(rateTimeItem));
        QCOMPARE(node(release, {1}).kind, Kind::Cell);
    }

    void moveFromAStalePathIsRefused()
    {
        const QJsonObject before = json(review);
        QVERIFY(!editor->drop(review, centre({0}), moved(itemRef(notesItem), work, {0})));
        QCOMPARE(json(review), before);
    }

    // A task dropped inside a task cell lands beside the task cell.
    void taskDroppedInATaskCellLandsBesideIt()
    {
        FakeFactory rootFactory;
        rootFactory.services.model = model.get();
        rootFactory.services.types = types.get();
        rootFactory.rootDashboard = morning;
        LayoutView rootView(&rootFactory);
        rootView.setTree(tree(morning));
        editor->attach(&rootView, morning);

        auto *taskCell = qobject_cast<TaskCell *>(rootView.cellAt({0}));
        QVERIFY(taskCell);
        FakeFactory nestedFactory;
        auto *nested = new LayoutView(&nestedFactory, taskCell);
        nested->setTree(tree(work));
        editor->attach(nested, work);

        auto *overlay = nested->findChild<DropOverlay *>();
        QVERIFY(overlay);
        emit overlay->dropped(centre({1, 1}), fromSidebar(taskRef(flakyTask)));
        QTRY_COMPARE(tree(morning).children.size(), std::size_t(3));
        QCOMPARE(refAt(morning, {1}), taskRef(flakyTask, model->task(flakyTask)->defaultDashboardId));
        QCOMPARE(tree(work).children.size(), std::size_t(2));

        // An item drop in the nested view stays in the task dashboard.
        emit overlay->dropped(centre({0}), fromSidebar(itemRef(notesItem, loginTask)));
        QTRY_COMPARE(node(work, {0}).kind, Kind::Tabs);

        editor->detach(nested);
        QVERIFY(!nested->findChild<DropOverlay *>());
    }

    void treeEditedIsStored()
    {
        FakeFactory factory;
        LayoutView view(&factory);
        view.setTree(tree(work));
        editor->attach(&view, work);
        emit view.treeEdited(layout::setCurrentTab(tree(work), {1}, 2));
        QCOMPARE(node(work, {1}).current, 2);
    }

    // Drag data

    void dragMimeRoundTrip()
    {
        const DragPayload payload = moved(itemRef(claudeItem, loginTask), work, {1, 0, 2});
        std::unique_ptr<QMimeData> mime(dragMimeData(payload));
        QVERIFY(mime->hasFormat(QString::fromLatin1(dragMimeType)));
        const std::optional<DragPayload> read = dragPayload(mime.get());
        QVERIFY(read);
        QCOMPARE(read->ref, payload.ref);
        QVERIFY(read->sourceDashboard);
        QCOMPARE(*read->sourceDashboard, work);
        QCOMPARE(read->sourcePath, payload.sourcePath);

        std::unique_ptr<QMimeData> sidebar(dragMimeData(fromSidebar(taskRef(flakyTask, QStringLiteral("d")))));
        const std::optional<DragPayload> fromSide = dragPayload(sidebar.get());
        QVERIFY(fromSide);
        QCOMPARE(fromSide->ref, taskRef(flakyTask, QStringLiteral("d")));
        QVERIFY(!fromSide->sourceDashboard);
        QVERIFY(!fromSide->sourcePath);
    }

    void dragMimeRejectsBadData()
    {
        QVERIFY(!dragPayload(nullptr));
        QMimeData empty;
        QVERIFY(!dragPayload(&empty));

        const QString type = QString::fromLatin1(dragMimeType);
        const auto check = [&](const char *data) {
            QMimeData mime;
            mime.setData(type, QByteArray(data));
            return dragPayload(&mime).has_value();
        };
        QVERIFY(!check("not json"));
        QVERIFY(!check("[1, 2]"));
        QVERIFY(!check("{}"));
        QVERIFY(!check(R"({"ref": "x"})"));
        QVERIFY(!check(R"({"ref": {"item": "a"}, "sourcePath": [0, -1]})"));
        QVERIFY(!check(R"({"ref": {"item": "a"}, "sourcePath": [0.5]})"));
        QVERIFY(!check(R"({"ref": {"item": "a"}, "sourcePath": 3})"));
        QVERIFY(!check(R"({"ref": {"item": "a"}, "sourceDashboard": {"taskId": "t"}})"));
        QVERIFY(check(R"({"ref": {"item": "a"}, "sourcePath": [0, 1]})"));
    }

    // Drop zones

    void dropZonesOfAShownView()
    {
        // claude | tabs[...]
        FakeFactory factory;
        LayoutView view(&factory);
        view.resize(800, 600);
        view.setTree(tree(work));
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));

        QWidget *cell = view.cellAt({0});
        const QRect rect(cell->mapTo(&view, QPoint(0, 0)), cell->size());
        const auto zoneAt = [&](double x, double y) {
            return dropHitAt(&view, QPoint(rect.left() + int(rect.width() * x), rect.top() + int(rect.height() * y)));
        };

        auto hit = zoneAt(0.5, 0.5);
        QVERIFY(hit);
        QCOMPARE(hit->target.path, NodePath{0});
        QCOMPARE(hit->target.zone.kind, DropZone::Kind::Centre);
        QVERIFY(rect.contains(hit->highlight));

        const auto checkEdge = [&](double x, double y, Edge expected) {
            const auto edgeHit = zoneAt(x, y);
            QVERIFY(edgeHit);
            QCOMPARE(edgeHit->target.zone.kind, DropZone::Kind::Edge);
            QCOMPARE(edgeHit->target.zone.edge, expected);
        };
        checkEdge(0.1, 0.5, Edge::Left);
        checkEdge(0.9, 0.5, Edge::Right);
        checkEdge(0.5, 0.1, Edge::Top);
        checkEdge(0.5, 0.9, Edge::Bottom);
        checkEdge(0.05, 0.2, Edge::Left);   // nearest edge wins

        // The tab bar: before the second tab's middle is index 1.
        auto *tabWidget = view.findChild<QTabWidget *>();
        QVERIFY(tabWidget);
        QTabBar *bar = tabWidget->tabBar();
        const QPoint second = bar->mapTo(&view, bar->tabRect(1).center());
        hit = dropHitAt(&view, second - QPoint(2, 0));
        QVERIFY(hit);
        QCOMPARE(hit->target.path, NodePath{1});
        QCOMPARE(hit->target.zone.kind, DropZone::Kind::TabBar);
        QCOMPARE(hit->target.zone.tabIndex, 1);
        hit = dropHitAt(&view, second + QPoint(2, 0));
        QVERIFY(hit);
        QCOMPARE(hit->target.zone.tabIndex, 2);
        // The free space after the last tab appends.
        const QRect tabArea(tabWidget->mapTo(&view, QPoint(0, 0)), tabWidget->size());
        hit = dropHitAt(&view, QPoint(tabArea.right() - 2, second.y()));
        QVERIFY(hit);
        QCOMPARE(hit->target.zone.tabIndex, 3);

        // Inside the current tab, the cell of the split under the cursor.
        QWidget *issue = view.cellAt({1, 0, 0});
        QVERIFY(issue && issue->isVisible());
        hit = dropHitAt(&view, issue->mapTo(&view, issue->rect().center()));
        QVERIFY(hit);
        QCOMPARE(hit->target.path, (NodePath{1, 0, 0}));
    }

    void overlayEmitsDropped()
    {
        FakeFactory factory;
        LayoutView view(&factory);
        view.resize(800, 600);
        view.setTree(tree(work));
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        auto *overlay = new DropOverlay(&view);
        QSignalSpy spy(overlay, &DropOverlay::dropped);

        QWidget *cell = view.cellAt({0});
        const QPoint pos = cell->mapTo(&view, cell->rect().center());
        std::unique_ptr<QMimeData> mime(dragMimeData(fromSidebar(itemRef(notesItem))));

        QDragEnterEvent enter(pos, Qt::CopyAction, mime.get(), Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&view, &enter);
        QVERIFY(enter.isAccepted());
        QVERIFY(overlay->isVisible());

        QDropEvent drop(QPointF(pos), Qt::CopyAction, mime.get(), Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&view, &drop);
        QVERIFY(!overlay->isVisible());
        QCOMPARE(spy.count(), 1);
        const auto target = spy.at(0).at(0).value<DropTarget>();
        QCOMPARE(target.path, NodePath{0});
        QCOMPARE(target.zone.kind, DropZone::Kind::Centre);

        // A cell move needs edit mode.
        std::unique_ptr<QMimeData> move(dragMimeData(moved(itemRef(claudeItem), work, {0})));
        QDragEnterEvent locked(pos, Qt::MoveAction, move.get(), Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&view, &locked);
        QVERIFY(!locked.isAccepted());
        QVERIFY(!overlay->isVisible());
    }
};

QTEST_MAIN(TestLayoutEditor)
#include "tst_layout_editor.moc"
