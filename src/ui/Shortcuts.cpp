#include "ui/Shortcuts.h"

#include "layout/CellFrame.h"
#include "layout/LayoutView.h"
#include "layout/TaskCell.h"
#include "ui/MainWindow.h"
#include "ui/RootTabBar.h"

#include <QApplication>
#include <QEvent>
#include <QKeySequence>
#include <QPointer>
#include <QRect>
#include <QShortcut>
#include <QTabWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <limits>
#include <tuple>
#include <vector>

namespace mirante {

namespace {

enum class Direction { Left, Right, Up, Down };

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

// The layer that holds a maximised cell over the content area. If it goes
// first, for example while the window closes, it gives the cell back.
class MaximiseLayer : public QWidget
{
public:
    explicit MaximiseLayer(QWidget *parent)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("maximiseLayer"));
        setAutoFillBackground(true);
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
    }

    ~MaximiseLayer() override
    {
        if (onDestroy)
            onDestroy();
    }

    std::function<void()> onDestroy;
};

// The cell frame that holds the widget, or nullptr. A frame that is being
// destroyed is no longer a CellFrame here, so it is never returned.
CellFrame *cellOf(QWidget *widget)
{
    for (QWidget *w = widget; w; w = w->parentWidget()) {
        if (auto *frame = qobject_cast<CellFrame *>(w))
            return frame;
        if (w->isWindow())
            break;
    }
    return nullptr;
}

template<typename T>
T *ancestorOf(const QWidget *widget)
{
    for (QWidget *w = widget ? widget->parentWidget() : nullptr; w; w = w->parentWidget()) {
        if (auto *found = qobject_cast<T *>(w))
            return found;
        if (w->isWindow())
            break;
    }
    return nullptr;
}

// The visible cell frames of a view, inside task cells too, in depth-first order.
void collectCells(const LayoutView *view, std::vector<QWidget *> &cells)
{
    for (QWidget *cell : view->visibleCells()) {
        if (qobject_cast<CellFrame *>(cell)) {
            cells.push_back(cell);
        } else if (auto *taskCell = qobject_cast<TaskCell *>(cell)) {
            if (const LayoutView *inner = taskCell->view())
                collectCells(inner, cells);
        }
    }
}

} // namespace

struct Shortcuts::Private
{
    Shortcuts *q = nullptr;
    MainWindow *window = nullptr;
    QPointer<QWidget> content;

    QPointer<QWidget> focused;              // a CellFrame
    QPointer<QWidget> maximised;            // the maximised cell, in the layer
    QPointer<LayoutView> maximisedView;     // the view it came from
    QPointer<MaximiseLayer> layer;
    std::vector<QMetaObject::Connection> maximisedConnections;

    std::vector<QPointer<QShortcut>> shortcuts;
    std::vector<QMetaObject::Connection> connections;
    EventWatcher watcher;

    std::vector<QWidget *> visibleCells() const
    {
        std::vector<QWidget *> cells;
        if (const LayoutView *view = window->currentLayoutView())
            collectCells(view, cells);
        return cells;
    }

    void setFocused(QWidget *cell)
    {
        if (focused == cell)
            return;
        if (auto *old = qobject_cast<CellFrame *>(focused.data()))
            old->setCellFocused(false);
        focused = cell;
        if (auto *frame = qobject_cast<CellFrame *>(cell))
            frame->setCellFocused(true);
    }

    void onFocusChanged(QWidget *now)
    {
        // Focus leaving the application, or going to a popup, keeps the mark.
        if (!now || now->window() != window)
            return;
        setFocused(cellOf(now));
    }

    void focusFirstCell(const QWidget *within = nullptr)
    {
        for (QWidget *cell : visibleCells()) {
            if (!within || within == cell || within->isAncestorOf(cell)) {
                q->focusCell(cell);
                return;
            }
        }
    }

    void move(Direction direction)
    {
        if (maximised)
            return;
        const std::vector<QWidget *> cells = visibleCells();
        if (cells.empty())
            return;
        QWidget *current = focused;
        if (!current || std::find(cells.begin(), cells.end(), current) == cells.end()) {
            q->focusCell(cells.front());
            return;
        }

        // Cells in line with the focused one come first: those that overlap it
        // across the direction of movement. Then the nearest along the axis,
        // then the smallest offset across it.
        const QRect from(current->mapTo(window, QPoint(0, 0)), current->size());
        const bool horizontal = direction == Direction::Left || direction == Direction::Right;
        QWidget *best = nullptr;
        std::tuple<bool, int, int> bestRank{true, std::numeric_limits<int>::max(), std::numeric_limits<int>::max()};
        for (QWidget *cell : cells) {
            if (cell == current)
                continue;
            const QRect to(cell->mapTo(window, QPoint(0, 0)), cell->size());
            const int dx = to.center().x() - from.center().x();
            const int dy = to.center().y() - from.center().y();
            int axis = 0;
            switch (direction) {
            case Direction::Left:
                axis = -dx;
                break;
            case Direction::Right:
                axis = dx;
                break;
            case Direction::Up:
                axis = -dy;
                break;
            case Direction::Down:
                axis = dy;
                break;
            }
            if (axis <= 0)
                continue;
            const bool inLine = horizontal ? to.top() <= from.bottom() && from.top() <= to.bottom()
                                           : to.left() <= from.right() && from.left() <= to.right();
            const std::tuple<bool, int, int> rank{!inLine, axis, std::abs(horizontal ? dy : dx)};
            if (rank < bestRank) {
                best = cell;
                bestRank = rank;
            }
        }
        if (best)
            q->focusCell(best);
    }

    void switchTab(int step)
    {
        auto *tabs = ancestorOf<QTabWidget>(focused);
        if (!tabs || tabs->count() < 2)
            return;
        const int count = tabs->count();
        tabs->setCurrentIndex(((tabs->currentIndex() + step) % count + count) % count);
        focusFirstCell(tabs->currentWidget());
    }

    void switchRootTab(int step)
    {
        RootTabBar *bar = window->rootTabBar();
        const int count = bar ? bar->count() : 0;
        if (count < 2)
            return;
        const int current = bar->currentIndex();
        for (int i = 1; i < count; ++i) {
            const int index = ((current + step * i) % count + count) % count;
            if (bar->isTabVisible(index)) {
                bar->setCurrentIndex(index);
                focusFirstCell();
                return;
            }
        }
    }

    void clearMaximisedConnections()
    {
        for (const QMetaObject::Connection &connection : maximisedConnections)
            QObject::disconnect(connection);
        maximisedConnections.clear();
    }

    void maximise(QWidget *cell)
    {
        LayoutView *view = ancestorOf<LayoutView>(cell);
        if (!cell || !view || !content)
            return;
        const auto path = view->pathOf(cell);
        if (!path)
            return;

        view->detachCell(*path);
        if (!layer) {
            layer = new MaximiseLayer(content);
            layer->onDestroy = [this] { restore(false); };
        }
        layer->layout()->addWidget(cell);
        cell->show();
        layer->setGeometry(content->rect());
        layer->raise();
        layer->show();
        maximised = cell;
        maximisedView = view;
        if (auto *frame = qobject_cast<CellFrame *>(cell))
            frame->setMaximised(true);

        // The view may delete the cell when it rebuilds. A view that goes
        // first leaves the cell to its new parent, so it goes here.
        maximisedConnections.push_back(QObject::connect(cell, &QObject::destroyed, q, [this] {
            clearMaximisedConnections();
            if (layer)
                layer->hide();
        }));
        maximisedConnections.push_back(QObject::connect(view, &QObject::destroyed, q, [this] {
            clearMaximisedConnections();
            delete maximised.data();
            if (layer)
                layer->hide();
        }));
        q->focusCell(cell);
    }

    void restore(bool refocus)
    {
        QWidget *cell = maximised;
        LayoutView *view = maximisedView;
        clearMaximisedConnections();
        maximised = nullptr;
        maximisedView = nullptr;
        if (auto *frame = qobject_cast<CellFrame *>(cell))
            frame->setMaximised(false);
        if (cell) {
            if (view)
                view->reattachCell(cell);
            else
                delete cell;
        }
        if (layer)
            layer->hide();
        if (cell && view && refocus)
            q->focusCell(cell);
    }

    // Restores the cell if it is the maximised one. Otherwise restores any
    // other maximised cell first, then maximises this one.
    void toggleMaximise(QWidget *cell)
    {
        if (!cell)
            return;
        if (cell == maximised) {
            restore(true);
            return;
        }
        if (maximised)
            restore(false);
        maximise(cell);
    }

    void onEvent(QObject *watched, QEvent *event)
    {
        if (event->type() == QEvent::Resize && watched == content && layer)
            layer->setGeometry(content->rect());
    }

    void add(const QKeySequence &keys, std::function<void()> action)
    {
        auto *shortcut = new QShortcut(keys, window);
        shortcut->setContext(Qt::WindowShortcut);
        QObject::connect(shortcut, &QShortcut::activated, q, std::move(action));
        shortcuts.emplace_back(shortcut);
    }
};

Shortcuts::Shortcuts(MainWindow *window)
    : QObject(window)
    , d(std::make_unique<Private>())
{
    d->q = this;
    d->window = window;
    d->content = window->contentArea();

    d->watcher.handler = [this](QObject *watched, QEvent *event) { d->onEvent(watched, event); };
    if (d->content)
        d->content->installEventFilter(&d->watcher);

    d->connections.push_back(connect(qApp, &QApplication::focusChanged, this,
                                     [this](QWidget * /*old*/, QWidget *now) { d->onFocusChanged(now); }));
    if (RootTabBar *bar = window->rootTabBar()) {
        d->connections.push_back(connect(bar, &QTabBar::currentChanged, this, [this] { d->restore(false); }));
    }

    d->add(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Left), [this] { d->move(Direction::Left); });
    d->add(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Right), [this] { d->move(Direction::Right); });
    d->add(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Up), [this] { d->move(Direction::Up); });
    d->add(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Down), [this] { d->move(Direction::Down); });
    d->add(QKeySequence(Qt::CTRL | Qt::Key_Tab), [this] { d->switchTab(1); });
    d->add(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Backtab), [this] { d->switchTab(-1); });
    d->add(QKeySequence(Qt::CTRL | Qt::Key_PageDown), [this] { d->switchRootTab(1); });
    d->add(QKeySequence(Qt::CTRL | Qt::Key_PageUp), [this] { d->switchRootTab(-1); });
    d->add(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_M), [this] {
        // The maximised cell may have lost the focus, for example to the tab bar.
        d->toggleMaximise(d->maximised ? d->maximised.data() : d->focused.data());
    });
    d->add(QKeySequence(Qt::CTRL | Qt::Key_W), [this] {
        d->restore(false);
        d->window->closeCurrentTab();
    });
}

Shortcuts::~Shortcuts()
{
    // Nothing that reads d may run after it goes: the signals, the event
    // filters and the layer's callback are all cut first.
    for (const QMetaObject::Connection &connection : d->connections)
        disconnect(connection);
    d->connections.clear();
    for (const QPointer<QShortcut> &shortcut : d->shortcuts)
        delete shortcut.data();
    d->shortcuts.clear();
    if (d->content)
        d->content->removeEventFilter(&d->watcher);
    d->watcher.handler = nullptr;

    d->restore(false);
    if (d->layer) {
        d->layer->onDestroy = nullptr;
        delete d->layer.data();
    }
}

QWidget *Shortcuts::focusedCell() const { return d->focused; }

void Shortcuts::focusCell(QWidget *cell)
{
    if (!cell)
        return;
    if (cell->focusPolicy() == Qt::NoFocus)
        cell->setFocusPolicy(Qt::StrongFocus);
    cell->setFocus(Qt::ShortcutFocusReason);
    d->setFocused(cellOf(cell));
}

void Shortcuts::toggleMaximised(QWidget *cell) { d->toggleMaximise(cell); }

bool Shortcuts::isMaximised(const QWidget *cell) const { return cell && d->maximised == cell; }

} // namespace mirante
