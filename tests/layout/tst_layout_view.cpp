// Tests LayoutView on hand-made trees with a fake CellFactory: building, the
// cell pool on updates, size and tab reports, detaching, single view, and the
// CellFrame placeholders, body, header widget and maximise button, remaking
// cells that cannot move, and the placements of DashboardCellFactory.
#include "core/Model.h"
#include "items/ItemTypeRegistry.h"
#include "items/Placement.h"
#include "items/UnknownItemType.h"
#include "layout/CellFactory.h"
#include "layout/CellFrame.h"
#include "layout/DashboardCellFactory.h"
#include "layout/LayoutServices.h"
#include "layout/LayoutView.h"
#include "layout/SingleViewTracker.h"

#include <mirante/ItemType.h>

#include <QApplication>
#include <QKeySequence>
#include <QLabel>
#include <QMouseEvent>
#include <QPointer>
#include <QPushButton>
#include <QSignalSpy>
#include <QSplitter>
#include <QTabBar>
#include <QTabWidget>
#include <QTest>
#include <QToolButton>

#include <set>
#include <utility>

using namespace mirante;

namespace {

CellRef itemRef(const QString &item) { return CellRef{item, QString(), QString()}; }

LayoutNode cell(const QString &item)
{
    LayoutNode node;
    node.ref = itemRef(item);
    return node;
}

LayoutNode split(Qt::Orientation dir, std::vector<LayoutNode> children, std::vector<double> sizes = {})
{
    LayoutNode node;
    node.kind = LayoutNode::Kind::Split;
    node.dir = dir;
    node.sizes = sizes.empty() ? std::vector<double>(children.size(), 1.0) : std::move(sizes);
    node.children = std::move(children);
    return node;
}

LayoutNode tabs(std::vector<LayoutNode> children, int current = 0)
{
    LayoutNode node;
    node.kind = LayoutNode::Kind::Tabs;
    for (std::size_t i = 0; i < children.size(); ++i)
        node.titles.push_back(QStringLiteral("Tab %1").arg(i + 1));
    node.children = std::move(children);
    node.current = current;
    return node;
}

// a | (b over tabs[c, d])
LayoutNode sampleTree()
{
    return split(Qt::Horizontal,
                 {cell(QStringLiteral("a")),
                  split(Qt::Vertical, {cell(QStringLiteral("b")), tabs({cell(QStringLiteral("c")), cell(QStringLiteral("d"))})})});
}

// Makes a QLabel named after the item, or a CellFrame when frames is set.
class FakeFactory : public CellFactory
{
public:
    QWidget *createCell(const std::optional<CellRef> &ref, bool removed, QWidget *parent) override
    {
        ++created;
        if (frameType)
            return new CellFrame(ref, removed, frameType, ref ? ref->item.toUpper() : QString(), parent);
        auto *label = new QLabel(ref ? ref->item : QStringLiteral("empty"), parent);
        label->setObjectName(ref ? ref->item : QStringLiteral("empty"));
        return label;
    }

    bool canReparent(const std::optional<CellRef> &ref) const override
    {
        return !ref || notReparentable.count(ref->item) == 0;
    }

    int created = 0;
    std::set<QString> notReparentable;
    const ItemType *frameType = nullptr;
};

class TestType : public ItemType
{
public:
    explicit TestType(bool single)
        : m_single(single)
    {
    }

    QString id() const override { return QStringLiteral("test.type"); }
    QString name() const override { return QStringLiteral("Test type"); }
    QIcon icon() const override { return QIcon(); }
    bool singleView() const override { return m_single; }

private:
    bool m_single;
};

QStringList labelTexts(const QWidget *widget)
{
    QStringList texts;
    for (const QLabel *label : widget->findChildren<QLabel *>())
        texts.append(label->text());
    return texts;
}

bool bodyContains(const QWidget *widget, const QString &text)
{
    for (const QString &label : labelTexts(widget)) {
        if (label.contains(text))
            return true;
    }
    return false;
}

QPushButton *showHereButton(const QWidget *frame)
{
    for (QPushButton *button : frame->findChildren<QPushButton *>()) {
        if (button->text() == QStringLiteral("Show here"))
            return button;
    }
    return nullptr;
}

// A type whose view is a label named "itemView", for the factory's placements.
class ViewType : public ItemType
{
public:
    QString id() const override { return QStringLiteral("view.type"); }
    QString name() const override { return QStringLiteral("View type"); }
    QIcon icon() const override { return QIcon(); }
    QWidget *createView(ItemContext &, QWidget *parent) const override
    {
        auto *label = new QLabel(QStringLiteral("the view"), parent);
        label->setObjectName(QStringLiteral("itemView"));
        return label;
    }
};

void registerViewType(ModuleRegistrar &registrar) { registrar.addItemType(std::make_unique<ViewType>()); }

QToolButton *toolButton(const QWidget *frame, const QString &toolTipStart)
{
    for (QToolButton *button : frame->findChildren<QToolButton *>()) {
        if (button->toolTip().startsWith(toolTipStart))
            return button;
    }
    return nullptr;
}

} // namespace

class TestLayoutView : public QObject
{
    Q_OBJECT

private slots:
    void buildsWidgets();
    void sizesOnlyKeepsWidgets();
    void structuralChangeReusesCells();
    void equalRefsKeepOrder();
    void notReparentableIsMadeAgain();
    void leftoverCellsAreDestroyed();
    void splitterMoveReportsAfterDelay();
    void tabChangeReportsAtOnce();
    void addTabButtonInEditMode();
    void tabBarDragsInEditMode();
    void cellSignalsCarryPath();
    void editModeReachesFrames();
    void detachAndReattach();
    void singleView();
    void frameTexts();
    void remakeUnsafeCells();
    void frameBody();
    void frameHeaderWidget();
    void maximiseButton();
    void factoryMakesPlacements();
};

void TestLayoutView::buildsWidgets()
{
    FakeFactory factory;
    LayoutView view(&factory);
    view.setTree(sampleTree());

    QWidget *a = view.cellAt({0});
    QWidget *b = view.cellAt({1, 0});
    QWidget *c = view.cellAt({1, 1, 0});
    QWidget *d = view.cellAt({1, 1, 1});
    QVERIFY(a && b && c && d);
    QCOMPARE(a->objectName(), QStringLiteral("a"));
    QCOMPARE(d->objectName(), QStringLiteral("d"));
    QVERIFY(qobject_cast<QSplitter *>(a->parentWidget()));
    QVERIFY(view.cellAt({1}) == nullptr);   // a split, not a cell

    QCOMPARE(view.pathOf(c), std::optional<NodePath>(NodePath{1, 1, 0}));
    QVERIFY(!view.pathOf(&view));

    const std::vector<QWidget *> visible = view.visibleCells();
    QCOMPARE(visible, (std::vector<QWidget *>{a, b, c}));
}

void TestLayoutView::sizesOnlyKeepsWidgets()
{
    FakeFactory factory;
    LayoutView view(&factory);
    view.resize(800, 600);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    view.setTree(sampleTree());
    QSignalSpy edited(&view, &LayoutView::treeEdited);

    QWidget *a = view.cellAt({0});
    QWidget *c = view.cellAt({1, 1, 0});
    QWidget *d = view.cellAt({1, 1, 1});
    auto *outer = qobject_cast<QSplitter *>(a->parentWidget());
    const int created = factory.created;

    LayoutNode tree = sampleTree();
    tree.sizes = {1.0, 3.0};
    tree.children[1].children[1].current = 1;
    view.setTree(tree);

    QCOMPARE(factory.created, created);
    QCOMPARE(view.cellAt({0}), a);
    QCOMPARE(view.cellAt({1, 1, 0}), c);
    QCOMPARE(view.cellAt({1, 1, 1}), d);
    QCOMPARE(a->parentWidget(), outer);

    QTRY_VERIFY(outer->sizes()[1] > 2 * outer->sizes()[0]);

    auto *tabWidget = d->parentWidget()->parentWidget();
    QCOMPARE(qobject_cast<QTabWidget *>(tabWidget)->currentIndex(), 1);
    QCOMPARE(view.visibleCells(), (std::vector<QWidget *>{a, view.cellAt({1, 0}), d}));

    QTest::qWait(400);
    QCOMPARE(edited.count(), 0);
}

void TestLayoutView::structuralChangeReusesCells()
{
    FakeFactory factory;
    LayoutView view(&factory);
    view.setTree(sampleTree());
    QWidget *a = view.cellAt({0});
    QWidget *b = view.cellAt({1, 0});
    QWidget *c = view.cellAt({1, 1, 0});
    QWidget *d = view.cellAt({1, 1, 1});
    QPointer<QWidget> outer = a->parentWidget();

    // Split a: a new cell e appears to its right.
    const EditResult result = layout::splitCell(sampleTree(), {0}, Edge::Right, itemRef(QStringLiteral("e")));
    const int created = factory.created;
    view.setTree(result.tree);
    QCOMPARE(factory.created, created + 1);
    QCOMPARE(view.cellAt({0, 0}), a);
    QCOMPARE(view.cellAt({0, 1})->objectName(), QStringLiteral("e"));
    QCOMPARE(view.cellAt({1, 0}), b);
    QCOMPARE(view.cellAt({1, 1, 0}), c);
    QCOMPARE(view.cellAt({1, 1, 1}), d);
    QVERIFY(outer.isNull());

    // Move d to the left of a.
    const EditResult moved = layout::moveNode(result.tree, {1, 1, 1}, {0, 0}, DropZone{DropZone::Kind::Edge, Edge::Left, 0});
    view.setTree(moved.tree);
    QCOMPARE(factory.created, created + 1);
    for (QWidget *widget : {a, b, c, d})
        QVERIFY(view.pathOf(widget).has_value());
    QCOMPARE(view.cellAt(moved.path), d);
}

void TestLayoutView::equalRefsKeepOrder()
{
    FakeFactory factory;
    LayoutView view(&factory);
    view.setTree(split(Qt::Horizontal, {cell(QStringLiteral("x")), cell(QStringLiteral("x"))}));
    QWidget *first = view.cellAt({0});
    QWidget *second = view.cellAt({1});
    QVERIFY(first != second);

    // Nest the pair one level deeper: both widgets stay, in the same order.
    view.setTree(split(Qt::Vertical, {split(Qt::Horizontal, {cell(QStringLiteral("x")), cell(QStringLiteral("x"))}),
                                      cell(QStringLiteral("y"))}));
    QCOMPARE(view.cellAt({0, 0}), first);
    QCOMPARE(view.cellAt({0, 1}), second);
}

void TestLayoutView::notReparentableIsMadeAgain()
{
    FakeFactory factory;
    factory.notReparentable.insert(QStringLiteral("a"));
    LayoutView view(&factory);
    view.setTree(sampleTree());
    QPointer<QWidget> a = view.cellAt({0});
    QWidget *b = view.cellAt({1, 0});

    // A sizes-only change keeps even a widget that cannot be moved.
    LayoutNode resized = sampleTree();
    resized.sizes = {2.0, 1.0};
    view.setTree(resized);
    QCOMPARE(view.cellAt({0}), a.data());

    view.setTree(layout::splitCell(sampleTree(), {1, 0}, Edge::Bottom, std::nullopt).tree);
    QVERIFY(a.isNull());
    QVERIFY(view.cellAt({0}) != nullptr);
    QCOMPARE(view.cellAt({0})->objectName(), QStringLiteral("a"));
    QCOMPARE(view.cellAt({1, 0, 0}), b);
}

void TestLayoutView::leftoverCellsAreDestroyed()
{
    FakeFactory factory;
    LayoutView view(&factory);
    view.setTree(sampleTree());
    QPointer<QWidget> a = view.cellAt({0});
    QWidget *b = view.cellAt({1, 0});

    view.setTree(layout::removeNode(sampleTree(), {0}));
    QVERIFY(a.isNull());
    QVERIFY(view.pathOf(b).has_value());
}

void TestLayoutView::splitterMoveReportsAfterDelay()
{
    FakeFactory factory;
    LayoutView view(&factory);
    view.resize(800, 600);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    view.setTree(sampleTree());
    QSignalSpy edited(&view, &LayoutView::treeEdited);

    auto *outer = qobject_cast<QSplitter *>(view.cellAt({0})->parentWidget());
    QTRY_VERIFY(outer->width() > 0);
    const QList<int> before = outer->sizes();
    const int total = before[0] + before[1];
    outer->setSizes({total / 4, total - total / 4});
    emit outer->splitterMoved(total / 4, 1);
    QCOMPARE(edited.count(), 0);

    QVERIFY(edited.wait(2000));
    QCOMPARE(edited.count(), 1);
    const auto tree = edited.at(0).at(0).value<LayoutNode>();
    QCOMPARE(tree.sizes.size(), std::size_t(2));
    QVERIFY(qAbs(tree.sizes[0] - 0.25) < 0.02);
    QVERIFY(qAbs(tree.sizes[1] - 0.75) < 0.02);
    QCOMPARE(view.tree().sizes, tree.sizes);

    // The owner passes the tree back: nothing is rebuilt or reported.
    QWidget *a = view.cellAt({0});
    view.setTree(tree);
    QCOMPARE(view.cellAt({0}), a);
    QTest::qWait(400);
    QCOMPARE(edited.count(), 1);
}

// In edit mode, dragging a tab drags that tab's content, and dragging the empty
// part of the tab bar drags the whole tab group.
void TestLayoutView::tabBarDragsInEditMode()
{
    FakeFactory factory;
    LayoutView view(&factory);
    view.resize(800, 600);
    view.setTree(sampleTree());
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    auto *tabWidget = qobject_cast<QTabWidget *>(view.cellAt({1, 1, 0})->parentWidget()->parentWidget());
    QVERIFY(tabWidget);
    QTabBar *bar = tabWidget->tabBar();
    QSignalSpy started(&view, &LayoutView::cellDragStarted);
    const auto drag = [bar](const QPoint &from) {
        QTest::mousePress(bar, Qt::LeftButton, {}, from);
        QMouseEvent move(QEvent::MouseMove, QPointF(from + QPoint(40, 0)), bar->mapToGlobal(QPointF(from + QPoint(40, 0))),
                         Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(bar, &move);
        QTest::mouseRelease(bar, Qt::LeftButton, {}, from + QPoint(40, 0));
    };
    const QPoint onTab = bar->tabRect(1).center();
    const QPoint empty(bar->tabRect(bar->count() - 1).right() + 30, bar->height() / 2);

    // Locked: no drag.
    drag(onTab);
    QCOMPARE(started.count(), 0);

    view.setEditMode(true);
    drag(onTab);
    QCOMPARE(started.count(), 1);
    QCOMPARE(started.at(0).at(0).value<NodePath>(), (NodePath{1, 1, 1}));
    QVERIFY(bar->width() > empty.x());
    drag(empty);
    QCOMPARE(started.count(), 2);
    QCOMPARE(started.at(1).at(0).value<NodePath>(), (NodePath{1, 1}));
    QCOMPARE(view.widgetAt({1, 1}), static_cast<QWidget *>(tabWidget));

    // Hovering shows the open hand in edit mode only.
    const auto hover = [bar](const QPoint &at) {
        QMouseEvent move(QEvent::MouseMove, QPointF(at), bar->mapToGlobal(QPointF(at)), Qt::NoButton, Qt::NoButton,
                         Qt::NoModifier);
        QApplication::sendEvent(bar, &move);
    };
    hover(empty);
    QCOMPARE(bar->cursor().shape(), Qt::OpenHandCursor);
    view.setEditMode(false);
    hover(onTab);
    QCOMPARE(bar->cursor().shape(), Qt::ArrowCursor);
}

// A tab group's + button shows only in edit mode, and reports the group's path.
void TestLayoutView::addTabButtonInEditMode()
{
    FakeFactory factory;
    LayoutView view(&factory);
    view.setTree(sampleTree());
    auto *tabWidget = qobject_cast<QTabWidget *>(view.cellAt({1, 1, 0})->parentWidget()->parentWidget());
    QVERIFY(tabWidget);
    auto *add = tabWidget->findChild<QToolButton *>(QStringLiteral("addTabButton"));
    QVERIFY(add);
    QVERIFY(add->isHidden());

    view.setEditMode(true);
    QVERIFY(!add->isHidden());
    // It sits just after the last tab.
    QTabBar *bar = tabWidget->tabBar();
    const QRect last = bar->tabRect(bar->count() - 1);
    QCOMPARE(add->geometry().left(), bar->mapTo(tabWidget, last.topRight()).x() + 2);
    QSignalSpy requested(&view, &LayoutView::tabAddRequested);
    add->click();
    QCOMPARE(requested.count(), 1);
    QCOMPARE(requested.at(0).at(0).value<NodePath>(), (NodePath{1, 1}));

    view.setEditMode(false);
    QVERIFY(add->isHidden());
}

void TestLayoutView::tabChangeReportsAtOnce()
{
    FakeFactory factory;
    LayoutView view(&factory);
    view.setTree(sampleTree());
    QSignalSpy edited(&view, &LayoutView::treeEdited);

    auto *tabWidget = qobject_cast<QTabWidget *>(view.cellAt({1, 1, 0})->parentWidget()->parentWidget());
    QVERIFY(tabWidget);
    tabWidget->setCurrentIndex(1);
    QCOMPARE(edited.count(), 1);
    const auto tree = edited.at(0).at(0).value<LayoutNode>();
    QCOMPARE(layout::nodeAt(tree, {1, 1})->current, 1);
    QCOMPARE(view.visibleCells().back(), view.cellAt({1, 1, 1}));
}

void TestLayoutView::cellSignalsCarryPath()
{
    TestType type(false);
    FakeFactory factory;
    factory.frameType = &type;
    LayoutView view(&factory);
    view.setTree(sampleTree());
    QSignalSpy menu(&view, &LayoutView::cellMenuRequested);
    QSignalSpy drag(&view, &LayoutView::cellDragStarted);

    auto *frame = qobject_cast<CellFrame *>(view.cellAt({1, 0}));
    QVERIFY(frame);
    emit frame->menuRequested(QPoint(5, 6));
    QCOMPARE(menu.count(), 1);
    QCOMPARE(menu.at(0).at(0).value<NodePath>(), (NodePath{1, 0}));
    QCOMPARE(menu.at(0).at(1).toPoint(), QPoint(5, 6));

    // After a rebuild the same frame reports its new path.
    view.setTree(layout::wrapInTabs(sampleTree(), {}, QStringLiteral("All")).tree);
    QCOMPARE(view.cellAt({0, 1, 0}), frame);
    emit frame->dragStarted();
    QCOMPARE(drag.count(), 1);
    QCOMPARE(drag.at(0).at(0).value<NodePath>(), (NodePath{0, 1, 0}));
}

void TestLayoutView::editModeReachesFrames()
{
    TestType type(false);
    FakeFactory factory;
    factory.frameType = &type;
    LayoutView view(&factory);
    view.setTree(sampleTree());
    view.setEditMode(true);
    QVERIFY(view.editMode());

    // A frame made after the change starts in edit mode too.
    view.setTree(layout::splitCell(sampleTree(), {0}, Edge::Right, itemRef(QStringLiteral("e"))).tree);
    auto *frame = qobject_cast<CellFrame *>(view.cellAt({0, 1}));
    QVERIFY(frame);
    QSignalSpy drag(frame, &CellFrame::dragStarted);

    QWidget *header = frame->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly).value(0);
    QVERIFY(header);
    QCOMPARE(header->cursor().shape(), Qt::OpenHandCursor);
    view.resize(600, 400);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QTest::mousePress(header, Qt::LeftButton, {}, QPoint(10, 5));
    QMouseEvent move(QEvent::MouseMove, QPointF(60, 5), header->mapToGlobal(QPointF(60, 5)), Qt::NoButton,
                     Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(header, &move);
    QCOMPARE(drag.count(), 1);
    QTest::mouseRelease(header, Qt::LeftButton, {}, QPoint(60, 5));

    view.setEditMode(false);
    QCOMPARE(header->cursor().shape(), Qt::ArrowCursor);
}

void TestLayoutView::detachAndReattach()
{
    FakeFactory factory;
    LayoutView view(&factory);
    view.resize(800, 600);
    view.show();
    view.setTree(sampleTree());
    QWidget *a = view.cellAt({0});
    QWidget *c = view.cellAt({1, 1, 0});
    auto *outer = qobject_cast<QSplitter *>(a->parentWidget());
    auto *tabWidget = qobject_cast<QTabWidget *>(c->parentWidget()->parentWidget());

    QCOMPARE(view.detachCell({0}), a);
    QVERIFY(outer->widget(0) != a);
    QVERIFY(a->parentWidget() != outer);
    QCOMPARE(view.cellAt({0}), a);
    QCOMPARE(view.detachCell({1, 1, 0}), c);
    QVERIFY(tabWidget->widget(0) != c);
    QCOMPARE(tabWidget->tabText(0), QStringLiteral("Tab 1"));

    // A sizes-only update keeps them detached.
    LayoutNode resized = sampleTree();
    resized.sizes = {3.0, 1.0};
    view.setTree(resized);
    QVERIFY(outer->widget(0) != a);

    view.reattachCell(a);
    QCOMPARE(outer->widget(0), a);
    QVERIFY(a->isVisible());
    view.reattachCell(c);
    QCOMPARE(tabWidget->widget(0), c);
    QCOMPARE(tabWidget->currentIndex(), 0);

    // A detached cell stays detached across a rebuild, then goes back to its new place.
    view.detachCell({0});
    const EditResult result = layout::splitCell(sampleTree(), {0}, Edge::Bottom, std::nullopt);
    view.setTree(result.tree);
    QCOMPARE(view.cellAt({0, 0}), a);
    auto *inner = qobject_cast<QSplitter *>(view.cellAt({0, 1})->parentWidget());
    QVERIFY(inner && inner->widget(0) != a);
    view.reattachCell(a);
    QCOMPARE(inner->widget(0), a);
}

void TestLayoutView::singleView()
{
    TestType type(true);
    FakeFactory factory;
    factory.frameType = &type;
    SingleViewTracker tracker;
    LayoutView view(&factory);
    view.setSingleViewTracker(&tracker);
    view.setTree(split(Qt::Horizontal, {cell(QStringLiteral("x")), cell(QStringLiteral("x"))}));
    QWidget *first = view.cellAt({0});
    QWidget *second = view.cellAt({1});
    QVERIFY(!tracker.holder(QStringLiteral("x")));   // nothing is shown yet

    view.resize(800, 600);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QWidget *holder = tracker.holder(QStringLiteral("x"));
    QVERIFY(holder == first || holder == second);
    QWidget *other = holder == first ? second : first;
    QPushButton *showHere = showHereButton(other);
    QVERIFY(showHere);
    QVERIFY(showHere->isVisible());
    QVERIFY(!showHereButton(holder)->isVisible());

    QSignalSpy changed(&tracker, &SingleViewTracker::holderChanged);
    QTest::mouseClick(showHere, Qt::LeftButton);
    QCOMPARE(tracker.holder(QStringLiteral("x")), other);
    QCOMPARE(changed.count(), 1);
    QVERIFY(!showHere->isVisible());
    QVERIFY(showHereButton(holder)->isVisible());

    // When the holder goes, the other shown cell takes the item.
    view.setTree(cell(QStringLiteral("x")));
    QWidget *remaining = view.cellAt({});
    QVERIFY(remaining == first || remaining == second);
    QTRY_COMPARE(tracker.holder(QStringLiteral("x")), remaining);
    QVERIFY(!showHereButton(remaining)->isVisible());
}

void TestLayoutView::frameTexts()
{
    TestType type(false);
    UnknownItemType unknown(QStringLiteral("timetrack.entry"));

    CellFrame empty(std::nullopt, false, nullptr, QString());
    QVERIFY(labelTexts(&empty).contains(QStringLiteral("Empty")));

    CellFrame removed(itemRef(QStringLiteral("gone")), true, nullptr, QString());
    QVERIFY(labelTexts(&removed).contains(QStringLiteral("Removed")));

    // Normalisation drops the reference of a removed cell.
    CellFrame normalised(std::nullopt, true, nullptr, QString());
    QVERIFY(labelTexts(&normalised).contains(QStringLiteral("Removed")));

    CellFrame missing(itemRef(QStringLiteral("i1")), false, &unknown, QStringLiteral("Hours"));
    QVERIFY(bodyContains(&missing, QStringLiteral("needs the timetrack module")));

    CellFrame item(itemRef(QStringLiteral("i2")), false, &type, QStringLiteral("My item"));
    QVERIFY(bodyContains(&item, QStringLiteral("Test type")));
    QVERIFY(bodyContains(&item, QStringLiteral("My item")));
    QVERIFY(bodyContains(&item, QStringLiteral("i2")));
    QCOMPARE(item.ref(), std::optional<CellRef>(itemRef(QStringLiteral("i2"))));

    QVERIFY(!item.isCellFocused());
    item.setCellFocused(true);
    QVERIFY(item.isCellFocused());
    QVERIFY(item.styleSheet().contains(item.palette().color(QPalette::Highlight).name()));
}

void TestLayoutView::remakeUnsafeCells()
{
    FakeFactory factory;
    factory.notReparentable.insert(QStringLiteral("a"));
    factory.notReparentable.insert(QStringLiteral("d"));
    LayoutView view(&factory);
    view.setTree(sampleTree());
    QPointer<QWidget> a = view.cellAt({0});
    QPointer<QWidget> d = view.cellAt({1, 1, 1});
    QWidget *b = view.cellAt({1, 0});
    QWidget *c = view.cellAt({1, 1, 0});
    const int created = factory.created;

    view.remakeUnsafeCells();
    QCOMPARE(factory.created, created + 2);
    QVERIFY(a.isNull());
    QVERIFY(d.isNull());

    // The new cells are in the same places; the others did not change.
    QWidget *newA = view.cellAt({0});
    QWidget *newD = view.cellAt({1, 1, 1});
    QVERIFY(newA && newD);
    QCOMPARE(newA->objectName(), QStringLiteral("a"));
    QCOMPARE(newD->objectName(), QStringLiteral("d"));
    auto *outer = qobject_cast<QSplitter *>(newA->parentWidget());
    QVERIFY(outer);
    QCOMPARE(outer->widget(0), newA);
    auto *tabWidget = qobject_cast<QTabWidget *>(view.widgetAt({1, 1}));
    QVERIFY(tabWidget);
    QCOMPARE(tabWidget->widget(1), newD);
    QCOMPARE(tabWidget->tabText(1), QStringLiteral("Tab 2"));
    QCOMPARE(tabWidget->currentIndex(), 0);
    QCOMPARE(view.cellAt({1, 0}), b);
    QCOMPARE(view.cellAt({1, 1, 0}), c);
    QCOMPARE(view.pathOf(newD), std::optional<NodePath>(NodePath{1, 1, 1}));

    // A single cell at the root is made again too.
    FakeFactory single;
    single.notReparentable.insert(QStringLiteral("a"));
    LayoutView alone(&single);
    alone.setTree(cell(QStringLiteral("a")));
    QPointer<QWidget> root = alone.cellAt({});
    alone.remakeUnsafeCells();
    QVERIFY(root.isNull());
    QVERIFY(alone.cellAt({}));
    QCOMPARE(alone.cellAt({})->parentWidget(), &alone);
}

void TestLayoutView::frameBody()
{
    TestType type(false);
    CellFrame frame(itemRef(QStringLiteral("i1")), false, &type, QStringLiteral("Item"));
    QVERIFY(!frame.body());
    QVERIFY(bodyContains(&frame, QStringLiteral("Id: i1")));

    auto *body = new QLabel(QStringLiteral("the body"));
    frame.setBody(body);
    QCOMPARE(frame.body(), body);
    QVERIFY(frame.isAncestorOf(body));
    QVERIFY(!bodyContains(&frame, QStringLiteral("Id: i1")));

    frame.resize(300, 200);
    frame.show();
    QVERIFY(body->isVisible());
}

void TestLayoutView::frameHeaderWidget()
{
    TestType type(false);
    CellFrame frame(itemRef(QStringLiteral("i1")), false, &type, QStringLiteral("Item"));
    frame.setMaximiseButtonVisible(true);
    frame.resize(500, 200);
    frame.show();
    QVERIFY(QTest::qWaitForWindowExposed(&frame));

    auto toolbar = std::make_unique<QLabel>(QStringLiteral("toolbar"));
    frame.setHeaderWidget(toolbar.get());
    QVERIFY(frame.isAncestorOf(toolbar.get()));
    QVERIFY(toolbar->isVisible());

    // Between the name and the menu button, which comes before the maximise button.
    QWidget *name = frame.findChild<QWidget *>(QStringLiteral("cellName"));
    QToolButton *menu = toolButton(&frame, QStringLiteral("Cell menu"));
    QToolButton *maximise = frame.findChild<QToolButton *>(QStringLiteral("maximiseButton"));
    QVERIFY(name && menu && maximise);
    QTRY_VERIFY(toolbar->width() > 0);
    const auto x = [&frame](const QWidget *widget) { return widget->mapTo(&frame, QPoint(0, 0)).x(); };
    QVERIFY(x(name) < x(toolbar.get()));
    QVERIFY(x(toolbar.get()) < x(menu));
    QVERIFY(x(menu) < x(maximise));

    // Removing it leaves it to its owner.
    frame.setHeaderWidget(nullptr);
    QVERIFY(!frame.isAncestorOf(toolbar.get()));
    QVERIFY(!toolbar->isVisible());

    // Deleted by its owner while in the header, it simply goes.
    auto *other = new QLabel(QStringLiteral("other"));
    frame.setHeaderWidget(other);
    delete other;
    frame.setHeaderWidget(toolbar.get());
    QVERIFY(frame.isAncestorOf(toolbar.get()));
    frame.setHeaderWidget(nullptr);
}

void TestLayoutView::maximiseButton()
{
    TestType type(false);
    CellFrame frame(itemRef(QStringLiteral("i1")), false, &type, QStringLiteral("Item"));
    auto *button = frame.findChild<QToolButton *>(QStringLiteral("maximiseButton"));
    QVERIFY(button);
    QVERIFY(!button->isVisibleTo(&frame));

    frame.setMaximiseButtonVisible(true);
    QVERIFY(button->isVisibleTo(&frame));
    const QString shortcut = QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_M).toString(QKeySequence::NativeText);
    QVERIFY(button->toolTip().startsWith(QStringLiteral("Maximise")));
    QVERIFY(button->toolTip().contains(shortcut));
    QVERIFY(!button->icon().isNull());

    QSignalSpy requested(&frame, &CellFrame::maximiseRequested);
    button->click();
    QCOMPARE(requested.count(), 1);

    frame.setMaximised(true);
    QVERIFY(button->toolTip().startsWith(QStringLiteral("Restore")));
    QVERIFY(button->toolTip().contains(shortcut));
    frame.setMaximised(false);
    QVERIFY(button->toolTip().startsWith(QStringLiteral("Maximise")));

    frame.setMaximiseButtonVisible(false);
    QVERIFY(!button->isVisibleTo(&frame));

    // Never on empty or removed cells.
    CellFrame empty(std::nullopt, false, nullptr, QString());
    empty.setMaximiseButtonVisible(true);
    QVERIFY(!empty.findChild<QToolButton *>(QStringLiteral("maximiseButton"))->isVisibleTo(&empty));
    CellFrame removed(itemRef(QStringLiteral("gone")), true, nullptr, QString());
    removed.setMaximiseButtonVisible(true);
    QVERIFY(!removed.findChild<QToolButton *>(QStringLiteral("maximiseButton"))->isVisibleTo(&removed));
}

void TestLayoutView::factoryMakesPlacements()
{
    ItemTypeRegistry types;
    types.registerModule(QStringLiteral("view"), &registerViewType);
    Model model;
    Group group;
    group.id = QStringLiteral("g1");
    Project project;
    project.id = QStringLiteral("p1");
    project.groupId = QStringLiteral("g1");
    Task task;
    task.id = QStringLiteral("t1");
    task.projectId = QStringLiteral("p1");
    Item item;
    item.id = QStringLiteral("i1");
    item.type = QStringLiteral("view.type");
    item.name = QStringLiteral("Item");
    task.items.push_back(item);
    model.reset({group}, {project}, {task}, {});

    PlacementServices placements;
    placements.model = &model;
    placements.types = &types;
    LayoutServices services;
    services.model = &model;
    services.types = &types;
    const DashboardRef dashboard{QStringLiteral("t1"), QStringLiteral("d1")};

    // Without placements the body stays a placeholder, and without the
    // callback there is no maximise button.
    {
        DashboardCellFactory factory(services, dashboard);
        std::unique_ptr<QWidget> cellWidget(factory.createCell(itemRef(QStringLiteral("i1")), false, nullptr));
        auto *frame = qobject_cast<CellFrame *>(cellWidget.get());
        QVERIFY(frame);
        QVERIFY(!frame->body());
        QVERIFY(!frame->findChild<QToolButton *>(QStringLiteral("maximiseButton"))->isVisibleTo(frame));
    }

    QWidget *toggled = nullptr;
    services.placements = &placements;
    services.toggleMaximised = [&toggled](QWidget *cell) { toggled = cell; };
    auto factory = std::make_unique<DashboardCellFactory>(services, dashboard);
    std::unique_ptr<QWidget> cellWidget(factory->createCell(itemRef(QStringLiteral("i1")), false, nullptr));
    // The callbacks are copied, so the cell works after the factory goes.
    factory.reset();
    auto *frame = qobject_cast<CellFrame *>(cellWidget.get());
    QVERIFY(frame);
    auto *placement = qobject_cast<Placement *>(frame->body());
    QVERIFY(placement);
    QCOMPARE(placement->taskId(), QStringLiteral("t1"));
    QCOMPARE(placement->itemId(), QStringLiteral("i1"));
    QVERIFY(!placement->view());

    frame->resize(300, 200);
    frame->show();
    QVERIFY(placement->view());
    QCOMPARE(placement->view()->objectName(), QStringLiteral("itemView"));

    // The placement's header widget goes to the frame's header.
    auto toolbar = std::make_unique<QLabel>(QStringLiteral("toolbar"));
    emit placement->headerWidgetChanged(toolbar.get());
    QVERIFY(frame->isAncestorOf(toolbar.get()));
    emit placement->headerWidgetChanged(nullptr);
    QVERIFY(!frame->isAncestorOf(toolbar.get()));

    auto *maximise = frame->findChild<QToolButton *>(QStringLiteral("maximiseButton"));
    QVERIFY(maximise->isVisible());
    maximise->click();
    QCOMPARE(toggled, frame);
}

QTEST_MAIN(TestLayoutView)
#include "tst_layout_view.moc"
