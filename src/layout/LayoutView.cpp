#include "layout/LayoutView.h"

#include "layout/CellFactory.h"
#include "layout/CellFrame.h"
#include "layout/CellFrameSingleView.h"
#include "layout/SingleViewTracker.h"
#include "layout/TaskCell.h"

#include <QApplication>
#include <QEvent>
#include <QMouseEvent>
#include <QPointer>
#include <QSplitter>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>
#include <map>
#include <set>

namespace mirante {

namespace {

constexpr int sizeDelayMs = 300;

// Where the tab at index is after QTabBar moved the tab at from to to.
int indexAfterMove(int index, int from, int to)
{
    if (index == from)
        return to;
    if (from < index && index <= to)
        return index - 1;
    if (to <= index && index < from)
        return index + 1;
    return index;
}

// Keeps a tab group's + button just after its last tab. In document mode the
// tab bar spans the whole group, so the position comes from the last tab's
// rectangle. The placer is a child of the bar, so it goes with it.
// Starts drags from a tab bar in edit mode: a tab drags its content once the
// mouse leaves the bar, and until then QTabBar moves it along the bar. The
// empty part of the bar drags the whole tab group. onDrag gets the tab index,
// or -1 for the empty part. It is a child of the bar, so it goes with it.
class TabBarDragWatcher : public QObject
{
public:
    TabBarDragWatcher(QTabBar *bar, std::function<bool()> editMode, std::function<void(int)> onDrag)
        : QObject(bar)
        , m_bar(bar)
        , m_editMode(std::move(editMode))
        , m_onDrag(std::move(onDrag))
    {
        // Hovering shows the open hand where a drag can start.
        bar->setMouseTracking(true);
        bar->installEventFilter(this);
        connect(bar, &QTabBar::tabMoved, this, [this](int from, int to) { m_tab = indexAfterMove(m_tab, from, to); });
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched != m_bar)
            return false;
        if (event->type() == QEvent::MouseButtonPress) {
            const auto *mouse = static_cast<QMouseEvent *>(event);
            m_pressed = mouse->button() == Qt::LeftButton && m_editMode();
            m_pressPos = mouse->position().toPoint();
            m_tab = m_bar->tabAt(m_pressPos);
        } else if (event->type() == QEvent::Enter
                   || (event->type() == QEvent::MouseMove && !m_pressed)) {
            if (m_editMode())
                m_bar->setCursor(Qt::OpenHandCursor);
            else
                m_bar->unsetCursor();
        } else if (event->type() == QEvent::MouseMove && m_pressed) {
            const auto *mouse = static_cast<QMouseEvent *>(event);
            const QPoint pos = mouse->position().toPoint();
            if ((m_tab < 0 || !m_bar->rect().contains(pos))
                && (pos - m_pressPos).manhattanLength() >= QApplication::startDragDistance()) {
                m_pressed = false;
                // The drag takes the real release, so the bar's own drag ends here.
                QMouseEvent release(QEvent::MouseButtonRelease, mouse->position(), mouse->globalPosition(),
                                    Qt::LeftButton, Qt::NoButton, mouse->modifiers());
                QApplication::sendEvent(m_bar, &release);
                m_onDrag(m_tab);
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            m_pressed = false;
        }
        return false;
    }

private:
    QTabBar *m_bar;
    std::function<bool()> m_editMode;
    std::function<void(int)> m_onDrag;
    bool m_pressed = false;
    QPoint m_pressPos;
    int m_tab = -1;
};

class AddTabPlacer : public QObject
{
public:
    AddTabPlacer(QTabBar *bar, QToolButton *button)
        : QObject(bar)
        , m_bar(bar)
        , m_button(button)
    {
        bar->installEventFilter(this);
        place();
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        // Tabs added, removed or renamed lead to a repaint, so Paint covers them.
        if (watched == m_bar && (event->type() == QEvent::Resize || event->type() == QEvent::Move
                                 || event->type() == QEvent::Show || event->type() == QEvent::Paint))
            place();
        return false;
    }

private:
    void place()
    {
        if (m_bar->count() == 0)
            return;
        const QRect last = m_bar->tabRect(m_bar->count() - 1);
        const QPoint corner = m_bar->mapTo(m_button->parentWidget(), last.topRight());
        m_button->adjustSize();
        const QPoint target(corner.x() + 2, corner.y() + (last.height() - m_button->height()) / 2);
        if (m_button->pos() != target)
            m_button->move(target);
        m_button->raise();
    }

    QTabBar *m_bar;
    QToolButton *m_button;
};

// Forwards the events of the objects it watches to a function.
class EventWatcher : public QObject
{
public:
    std::function<void(QObject *, QEvent *)> handler;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (handler)
            handler(watched, event);
        return QObject::eventFilter(watched, event);
    }
};

int clampIndex(int index, int count) { return count <= 0 ? -1 : std::clamp(index, 0, count - 1); }

int splitterLength(const QSplitter *splitter)
{
    return splitter->orientation() == Qt::Horizontal ? splitter->width() : splitter->height();
}

// A cell widget taken out of the old widgets, waiting to be placed again.
struct PoolEntry
{
    std::optional<CellRef> ref;
    bool removed = false;
    QWidget *widget = nullptr;
};

} // namespace

struct LayoutView::Private
{
    struct SplitInfo
    {
        NodePath path;
        std::vector<double> ratios;
        bool pending = true;    // the ratios wait for the splitter's first real size
    };

    LayoutView *q = nullptr;
    CellFactory *factory = nullptr;
    LayoutNode tree;
    bool editMode = false;
    QPointer<SingleViewTracker> tracker;

    QVBoxLayout *layout = nullptr;
    QWidget *root = nullptr;                        // the widget of the tree's root
    std::map<NodePath, QWidget *> cells;            // cell widgets, detached ones included
    std::map<NodePath, QWidget *> containers;       // splitters and tab widgets
    std::map<QSplitter *, SplitInfo> splits;
    std::map<QTabWidget *, NodePath> tabs;
    std::map<QWidget *, QWidget *> detached;        // detached cell to its placeholder

    std::set<QSplitter *> moved;                    // splitters moved by the user since the last report
    QTimer sizeTimer;
    EventWatcher watcher;

    void rebuild(const LayoutNode &newTree);
    QWidget *build(const LayoutNode &node, NodePath &path, std::vector<PoolEntry> &pool);
    void setUpNewCell(QWidget *cell);
    void applyInPlace(const LayoutNode &node, NodePath &path);
    void applySizes(QSplitter *splitter);
    void onSplitterEvent(QSplitter *splitter, QEvent *event);
    void reportSizes();
    void replace(const NodePath &path, QWidget *current, QWidget *replacement);
    void blockContainerSignals();
    void onTabMoved(QTabWidget *tabWidget, int from, int to);
};

void LayoutView::Private::blockContainerSignals()
{
    for (const auto &[path, container] : containers)
        container->blockSignals(true);
}

void LayoutView::Private::rebuild(const LayoutNode &newTree)
{
    // A pending size report belongs to the old widgets, so it is dropped.
    sizeTimer.stop();
    moved.clear();
    blockContainerSignals();

    // Take the cell widgets out of the old widgets, in depth-first order, so
    // that equal references keep their order.
    std::vector<PoolEntry> pool;
    std::set<QWidget *> pooled;
    for (const NodePath &path : layout::cellPaths(tree)) {
        const auto found = cells.find(path);
        if (found == cells.end() || !found->second)
            continue;
        QWidget *widget = found->second;
        const LayoutNode *node = layout::nodeAt(tree, path);
        const bool isDetached = detached.count(widget) > 0;
        if (!isDetached && !factory->canReparent(node->ref)) {
            delete widget;
            continue;
        }
        if (!isDetached && widget->parentWidget() != q)
            widget->setParent(q);
        pool.push_back({node->ref, node->removed, widget});
        pooled.insert(widget);
    }

    QPointer<QWidget> oldRoot = root;
    if (oldRoot)
        layout->removeWidget(oldRoot);
    cells.clear();
    containers.clear();
    splits.clear();
    tabs.clear();

    tree = newTree;
    NodePath path;
    root = build(tree, path, pool);

    if (oldRoot && pooled.count(oldRoot) == 0)
        delete oldRoot.data();
    for (const PoolEntry &entry : pool) {
        if (entry.widget) {
            detached.erase(entry.widget);
            delete entry.widget;
        }
    }

    layout->addWidget(root);
    root->show();
}

QWidget *LayoutView::Private::build(const LayoutNode &node, NodePath &path, std::vector<PoolEntry> &pool)
{
    switch (node.kind) {
    case LayoutNode::Kind::Cell: {
        QWidget *cell = nullptr;
        for (PoolEntry &entry : pool) {
            if (entry.widget && entry.removed == node.removed && entry.ref == node.ref) {
                cell = entry.widget;
                entry.widget = nullptr;
                break;
            }
        }
        if (!cell) {
            cell = factory->createCell(node.ref, node.removed, q);
            setUpNewCell(cell);
        }
        cells[path] = cell;

        // A detached cell stays detached. A new placeholder takes its place.
        const auto found = detached.find(cell);
        if (found != detached.end()) {
            found->second = new QWidget(q);
            return found->second;
        }
        return cell;
    }

    case LayoutNode::Kind::Split: {
        auto *splitter = new QSplitter(node.dir, q);
        splitter->setChildrenCollapsible(false);
        containers[path] = splitter;
        splits[splitter] = SplitInfo{path, node.sizes, true};
        for (int i = 0; i < int(node.children.size()); ++i) {
            path.push_back(i);
            QWidget *child = build(node.children[std::size_t(i)], path, pool);
            path.pop_back();
            splitter->addWidget(child);
            // A widget hidden as a tab page before stays hidden unless shown.
            child->show();
        }
        applySizes(splitter);
        splitter->installEventFilter(&watcher);
        QObject::connect(splitter, &QSplitter::splitterMoved, q, [this, splitter] {
            const auto found = splits.find(splitter);
            if (found == splits.end())
                return;
            found->second.pending = false;
            moved.insert(splitter);
            sizeTimer.start();
        });
        return splitter;
    }

    case LayoutNode::Kind::Tabs: {
        auto *tabWidget = new QTabWidget(q);
        tabWidget->setDocumentMode(true);
        tabWidget->setTabBarAutoHide(node.collapseSingle);
        containers[path] = tabWidget;
        tabs[tabWidget] = path;
        for (int i = 0; i < int(node.children.size()); ++i) {
            path.push_back(i);
            QWidget *child = build(node.children[std::size_t(i)], path, pool);
            path.pop_back();
            const QString title = std::size_t(i) < node.titles.size() ? node.titles[std::size_t(i)] : QString();
            tabWidget->addTab(child, title);
        }
        tabWidget->setCurrentIndex(clampIndex(node.current, tabWidget->count()));

        QObject::connect(tabWidget, &QTabWidget::currentChanged, q, [this, tabWidget](int index) {
            const auto found = tabs.find(tabWidget);
            if (found == tabs.end() || index < 0)
                return;
            tree = layout::setCurrentTab(tree, found->second, index);
            emit q->treeEdited(tree);
        });
        QTabBar *bar = tabWidget->tabBar();
        // Unlocked, a tab moves along its bar. Its page moves with it, so no
        // cell is made again.
        bar->setMovable(editMode);
        QObject::connect(bar, &QTabBar::tabMoved, q,
                         [this, tabWidget](int from, int to) { onTabMoved(tabWidget, from, to); });
        bar->setContextMenuPolicy(Qt::CustomContextMenu);
        QObject::connect(bar, &QWidget::customContextMenuRequested, q, [this, tabWidget, bar](const QPoint &pos) {
            const auto found = tabs.find(tabWidget);
            if (found == tabs.end())
                return;
            emit q->tabMenuRequested(found->second, bar->tabAt(pos), bar->mapToGlobal(pos));
        });

        // In edit mode a + button right after the last tab adds a tab.
        auto *add = new QToolButton(tabWidget);
        add->setObjectName(QStringLiteral("addTabButton"));
        add->setText(QStringLiteral("+"));
        add->setToolTip(LayoutView::tr("Add tab"));
        add->setAutoRaise(true);
        add->setVisible(editMode);
        new AddTabPlacer(bar, add);
        new TabBarDragWatcher(
            bar, [this] { return editMode; },
            [this, tabWidget](int tab) {
                const auto found = tabs.find(tabWidget);
                if (found == tabs.end())
                    return;
                NodePath dragged = found->second;
                if (tab >= 0)
                    dragged.push_back(tab);
                emit q->cellDragStarted(dragged);
            });
        QObject::connect(add, &QToolButton::clicked, q, [this, tabWidget] {
            const auto found = tabs.find(tabWidget);
            if (found != tabs.end())
                emit q->tabAddRequested(found->second);
        });
        return tabWidget;
    }
    }
    return nullptr;
}

void LayoutView::Private::setUpNewCell(QWidget *cell)
{
    if (auto *frame = qobject_cast<CellFrame *>(cell)) {
        frame->setEditMode(editMode);
        QObject::connect(frame, &CellFrame::menuRequested, q, [this, frame](const QPoint &globalPos) {
            if (const auto path = q->pathOf(frame))
                emit q->cellMenuRequested(*path, globalPos);
        });
        QObject::connect(frame, &CellFrame::dragStarted, q, [this, frame] {
            if (const auto path = q->pathOf(frame))
                emit q->cellDragStarted(*path);
        });
    }
    if (auto *taskCell = qobject_cast<TaskCell *>(cell)) {
        taskCell->setEditMode(editMode);
        QObject::connect(taskCell, &TaskCell::menuRequested, q, [this, taskCell](const QPoint &globalPos) {
            if (const auto path = q->pathOf(taskCell))
                emit q->cellMenuRequested(*path, globalPos);
        });
        QObject::connect(taskCell, &TaskCell::dragStarted, q, [this, taskCell] {
            if (const auto path = q->pathOf(taskCell))
                emit q->cellDragStarted(*path);
        });
    }
    if (auto *singleView = CellFrameSingleView::of(cell))
        singleView->setTracker(tracker);
}

void LayoutView::Private::applyInPlace(const LayoutNode &node, NodePath &path)
{
    if (node.kind == LayoutNode::Kind::Split) {
        if (auto *splitter = qobject_cast<QSplitter *>(containers[path])) {
            SplitInfo &info = splits[splitter];
            info.ratios = node.sizes;
            // A splitter the user is dragging keeps its sizes until it reports them.
            if (moved.count(splitter) == 0) {
                const QSignalBlocker blocker(splitter);
                applySizes(splitter);
            }
        }
    } else if (node.kind == LayoutNode::Kind::Tabs) {
        if (auto *tabWidget = qobject_cast<QTabWidget *>(containers[path])) {
            const QSignalBlocker blocker(tabWidget);
            tabWidget->setCurrentIndex(clampIndex(node.current, tabWidget->count()));
        }
    }
    for (int i = 0; i < int(node.children.size()); ++i) {
        path.push_back(i);
        applyInPlace(node.children[std::size_t(i)], path);
        path.pop_back();
    }
}

void LayoutView::Private::applySizes(QSplitter *splitter)
{
    const auto found = splits.find(splitter);
    if (found == splits.end())
        return;
    const int count = splitter->count();
    if (count == 0)
        return;

    std::vector<double> ratios = found->second.ratios;
    double total = 0;
    for (double &ratio : ratios) {
        ratio = std::max(ratio, 0.0);
        total += ratio;
    }
    if (int(ratios.size()) != count || total <= 0) {
        ratios.assign(std::size_t(count), 1.0);
        total = count;
    }

    // Before the splitter has a size, any length keeps the proportions until
    // the first resize applies them again.
    const int handles = (count - 1) * splitter->handleWidth();
    const int length = std::max(splitterLength(splitter) - handles, count);
    QList<int> sizes;
    for (double ratio : ratios)
        sizes.append(int(ratio / total * length + 0.5));
    splitter->setSizes(sizes);
}

void LayoutView::Private::onSplitterEvent(QSplitter *splitter, QEvent *event)
{
    if (event->type() != QEvent::Resize && event->type() != QEvent::Show)
        return;
    const auto found = splits.find(splitter);
    if (found == splits.end() || !found->second.pending || moved.count(splitter) > 0)
        return;
    applySizes(splitter);
    // A resize while shown is the real size, so the ratios are in place.
    if (event->type() == QEvent::Resize && splitter->isVisible() && splitterLength(splitter) > 0)
        found->second.pending = false;
}

void LayoutView::Private::reportSizes()
{
    if (moved.empty())
        return;
    LayoutNode edited = tree;
    for (QSplitter *splitter : moved) {
        const auto found = splits.find(splitter);
        if (found == splits.end())
            continue;
        const QList<int> sizes = splitter->sizes();
        double total = 0;
        for (int size : sizes)
            total += size;
        if (total <= 0)
            continue;
        std::vector<double> ratios;
        for (int size : sizes)
            ratios.push_back(size / total);
        found->second.ratios = ratios;
        edited = layout::setSizes(edited, found->second.path, ratios);
    }
    moved.clear();
    tree = edited;
    emit q->treeEdited(tree);
}

// A tab bar has moved a tab and its page. The tree and every path through that
// tab group follow, so the widgets stay as they are.
void LayoutView::Private::onTabMoved(QTabWidget *tabWidget, int from, int to)
{
    const auto found = tabs.find(tabWidget);
    if (found == tabs.end())
        return;
    const NodePath tabsPath = found->second;
    const auto pathAfterMove = [&](NodePath path) {
        if (path.size() > tabsPath.size() && std::equal(tabsPath.begin(), tabsPath.end(), path.begin()))
            path[tabsPath.size()] = indexAfterMove(path[tabsPath.size()], from, to);
        return path;
    };
    const auto widgetsAfterMove = [&](const std::map<NodePath, QWidget *> &widgets) {
        std::map<NodePath, QWidget *> result;
        for (const auto &[path, widget] : widgets)
            result[pathAfterMove(path)] = widget;
        return result;
    };
    cells = widgetsAfterMove(cells);
    containers = widgetsAfterMove(containers);
    for (auto &[widget, path] : tabs)
        path = pathAfterMove(path);
    for (auto &[splitter, info] : splits)
        info.path = pathAfterMove(info.path);
    tree = layout::moveTab(tree, tabsPath, from, to);
    emit q->treeEdited(tree);
}

// Puts replacement where current is, in the container of the node at path.
void LayoutView::Private::replace(const NodePath &path, QWidget *current, QWidget *replacement)
{
    if (path.empty()) {
        layout->replaceWidget(current, replacement);
        if (current->parentWidget() != q)
            current->setParent(q);
        else
            current->hide();
        root = replacement;
        replacement->show();
        return;
    }

    const NodePath parentPath(path.begin(), path.end() - 1);
    const int index = path.back();
    QWidget *container = containers[parentPath];
    if (auto *splitter = qobject_cast<QSplitter *>(container)) {
        const QSignalBlocker blocker(splitter);
        splitter->replaceWidget(index, replacement);
        current->setParent(q);
        replacement->show();
    } else if (auto *tabWidget = qobject_cast<QTabWidget *>(container)) {
        const QSignalBlocker blocker(tabWidget);
        const int currentIndex = tabWidget->currentIndex();
        const QString title = tabWidget->tabText(index);
        tabWidget->removeTab(index);
        current->setParent(q);
        tabWidget->insertTab(index, replacement, title);
        tabWidget->setCurrentIndex(currentIndex);
    }
}

LayoutView::LayoutView(CellFactory *factory, QWidget *parent)
    : QWidget(parent)
    , d(std::make_unique<Private>())
{
    d->q = this;
    d->factory = factory;
    d->layout = new QVBoxLayout(this);
    d->layout->setContentsMargins(0, 0, 0, 0);
    d->layout->setSpacing(0);

    d->sizeTimer.setSingleShot(true);
    d->sizeTimer.setInterval(sizeDelayMs);
    connect(&d->sizeTimer, &QTimer::timeout, this, [this] { d->reportSizes(); });

    d->watcher.handler = [this](QObject *watched, QEvent *event) {
        if (auto *splitter = qobject_cast<QSplitter *>(watched))
            d->onSplitterEvent(splitter, event);
    };

    d->tree = layout::emptyLayout();
    d->rebuild(d->tree);
}

LayoutView::~LayoutView()
{
    // The widgets go before d, so that their signals do not reach it.
    d->sizeTimer.stop();
    d->blockContainerSignals();
    d->watcher.handler = nullptr;
    std::vector<QWidget *> detachedCells;
    for (const auto &[cell, placeholder] : d->detached)
        detachedCells.push_back(cell);
    delete d->root;
    for (QWidget *cell : detachedCells) {
        // A detached cell still owned by the view goes with it. One that was
        // moved elsewhere is left to its new parent.
        if (cell->parentWidget() == this)
            delete cell;
    }
    d->detached.clear();
    d->cells.clear();
}

void LayoutView::setTree(const LayoutNode &tree)
{
    if (layout::sameShape(d->tree, tree)) {
        d->tree = tree;
        NodePath path;
        d->applyInPlace(d->tree, path);
        return;
    }
    d->rebuild(tree);
}

const LayoutNode &LayoutView::tree() const { return d->tree; }

void LayoutView::setEditMode(bool on)
{
    d->editMode = on;
    for (const auto &[tabWidget, path] : d->tabs) {
        tabWidget->tabBar()->setMovable(on);
        if (auto *add = tabWidget->findChild<QToolButton *>(QStringLiteral("addTabButton")))
            add->setVisible(on);
    }
    for (const auto &[path, cell] : d->cells) {
        if (auto *frame = qobject_cast<CellFrame *>(cell))
            frame->setEditMode(on);
        else if (auto *taskCell = qobject_cast<TaskCell *>(cell))
            taskCell->setEditMode(on);
    }
}

bool LayoutView::editMode() const { return d->editMode; }

void LayoutView::setSingleViewTracker(SingleViewTracker *tracker)
{
    d->tracker = tracker;
    for (const auto &[path, cell] : d->cells) {
        if (auto *singleView = CellFrameSingleView::of(cell))
            singleView->setTracker(tracker);
    }
}

QWidget *LayoutView::cellAt(const NodePath &path) const
{
    const auto found = d->cells.find(path);
    return found == d->cells.end() ? nullptr : found->second;
}

QWidget *LayoutView::widgetAt(const NodePath &path) const
{
    if (QWidget *cell = cellAt(path))
        return cell;
    const auto found = d->containers.find(path);
    return found != d->containers.end() ? found->second : nullptr;
}

std::optional<NodePath> LayoutView::pathOf(const QWidget *cell) const
{
    if (!cell)
        return std::nullopt;
    for (const auto &[path, widget] : d->cells) {
        if (widget == cell)
            return path;
    }
    return std::nullopt;
}

std::vector<QWidget *> LayoutView::visibleCells() const
{
    std::vector<QWidget *> result;
    NodePath path;
    std::function<void(const LayoutNode &)> visit = [&](const LayoutNode &node) {
        if (node.kind == LayoutNode::Kind::Cell) {
            if (QWidget *cell = cellAt(path))
                result.push_back(cell);
            return;
        }
        const int count = int(node.children.size());
        int first = 0;
        int last = count - 1;
        if (node.kind == LayoutNode::Kind::Tabs)
            first = last = clampIndex(node.current, count);
        for (int i = first; i >= 0 && i <= last; ++i) {
            path.push_back(i);
            visit(node.children[std::size_t(i)]);
            path.pop_back();
        }
    };
    visit(d->tree);
    return result;
}

QWidget *LayoutView::detachCell(const NodePath &path)
{
    QWidget *cell = cellAt(path);
    if (!cell || d->detached.count(cell) > 0)
        return cell;
    auto *placeholder = new QWidget(this);
    d->detached[cell] = placeholder;
    d->replace(path, cell, placeholder);
    return cell;
}

void LayoutView::reattachCell(QWidget *cell)
{
    const auto found = d->detached.find(cell);
    if (found == d->detached.end())
        return;
    QWidget *placeholder = found->second;
    d->detached.erase(found);
    const auto path = pathOf(cell);
    if (!path || !placeholder) {
        delete placeholder;
        return;
    }
    if (cell->parentWidget() != this)
        cell->setParent(this);
    d->replace(*path, placeholder, cell);
    delete placeholder;
}

void LayoutView::remakeUnsafeCells()
{
    // Copied first, because replacing a cell changes the map.
    const std::map<NodePath, QWidget *> current = d->cells;
    for (const auto &[path, cell] : current) {
        const LayoutNode *node = layout::nodeAt(d->tree, path);
        // A detached cell, such as a maximised one, did not move with the view.
        if (!cell || !node || d->detached.count(cell) > 0 || d->factory->canReparent(node->ref))
            continue;
        QWidget *replacement = d->factory->createCell(node->ref, node->removed, this);
        d->setUpNewCell(replacement);
        d->replace(path, cell, replacement);
        d->cells[path] = replacement;
        delete cell;
    }
}

} // namespace mirante
