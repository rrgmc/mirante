// Tests LayoutView on hand-made trees with a fake CellFactory: building, the
// cell pool on updates, size and tab reports, detaching, single view, and the
// CellFrame placeholders.
#include "items/UnknownItemType.h"
#include "layout/CellFactory.h"
#include "layout/CellFrame.h"
#include "layout/LayoutView.h"
#include "layout/SingleViewTracker.h"

#include <mirante/ItemType.h>

#include <QApplication>
#include <QLabel>
#include <QMouseEvent>
#include <QPointer>
#include <QPushButton>
#include <QSignalSpy>
#include <QSplitter>
#include <QTabWidget>
#include <QTest>

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
    void cellSignalsCarryPath();
    void editModeReachesFrames();
    void detachAndReattach();
    void singleView();
    void frameTexts();
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

QTEST_MAIN(TestLayoutView)
#include "tst_layout_view.moc"
