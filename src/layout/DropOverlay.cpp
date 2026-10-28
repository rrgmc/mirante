#include "layout/DropOverlay.h"

#include "layout/DropOverlayTarget.h"
#include "layout/LayoutView.h"

#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QPainter>
#include <QPointer>
#include <QSplitter>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <vector>

namespace mirante {

namespace {

constexpr double edgeShare = 0.25;
constexpr int markerWidth = 4;

QRect rectIn(const QWidget *widget, const QWidget *ancestor)
{
    return QRect(widget->mapTo(ancestor, QPoint(0, 0)), widget->size());
}

// The insert position and marker for pos over a tab bar.
DropHit tabBarHit(const LayoutView *view, const QTabWidget *tabWidget, const NodePath &tabsPath, const QPoint &pos)
{
    const QTabBar *bar = tabWidget->tabBar();
    const QPoint barOrigin = bar->mapTo(view, QPoint(0, 0));
    const int x = pos.x() - barOrigin.x();
    const int count = bar->count();

    int index = count;
    int markerX = 0;
    for (int i = 0; i < count; ++i) {
        const QRect tab = bar->tabRect(i);
        if (x < tab.center().x()) {
            index = i;
            markerX = tab.left();
            break;
        }
        markerX = tab.right() + 1;
    }

    DropHit hit;
    hit.target.path = tabsPath;
    hit.target.zone.kind = DropZone::Kind::TabBar;
    hit.target.zone.tabIndex = index;
    hit.highlight = QRect(barOrigin.x() + markerX - markerWidth / 2, barOrigin.y(), markerWidth, bar->height());
    return hit;
}

DropHit cellHit(const NodePath &path, const QRect &rect, const QPoint &pos)
{
    const double x = double(pos.x() - rect.left()) / std::max(rect.width(), 1);
    const double y = double(pos.y() - rect.top()) / std::max(rect.height(), 1);

    DropHit hit;
    hit.target.path = path;
    hit.target.zone.kind = DropZone::Kind::Centre;
    double nearest = edgeShare;
    const auto consider = [&](double distance, Edge edge) {
        if (distance < nearest) {
            nearest = distance;
            hit.target.zone.kind = DropZone::Kind::Edge;
            hit.target.zone.edge = edge;
        }
    };
    consider(x, Edge::Left);
    consider(1 - x, Edge::Right);
    consider(y, Edge::Top);
    consider(1 - y, Edge::Bottom);

    const int halfWidth = rect.width() / 2;
    const int halfHeight = rect.height() / 2;
    if (hit.target.zone.kind == DropZone::Kind::Centre) {
        hit.highlight = rect.adjusted(rect.width() / 5, rect.height() / 5, -rect.width() / 5, -rect.height() / 5);
    } else {
        switch (hit.target.zone.edge) {
        case Edge::Left:
            hit.highlight = QRect(rect.left(), rect.top(), halfWidth, rect.height());
            break;
        case Edge::Right:
            hit.highlight = QRect(rect.left() + halfWidth, rect.top(), rect.width() - halfWidth, rect.height());
            break;
        case Edge::Top:
            hit.highlight = QRect(rect.left(), rect.top(), rect.width(), halfHeight);
            break;
        case Edge::Bottom:
            hit.highlight = QRect(rect.left(), rect.top() + halfHeight, rect.width(), rect.height() - halfHeight);
            break;
        }
    }
    return hit;
}

} // namespace

std::optional<DropHit> dropHitAt(const LayoutView *view, const QPoint &pos, bool replace)
{
    for (QWidget *cell : view->visibleCells()) {
        // A detached cell is shown elsewhere.
        if (!cell->isVisible() || !view->isAncestorOf(cell))
            continue;
        const std::optional<NodePath> path = view->pathOf(cell);
        if (!path)
            continue;

        // Each node above the cell has one container widget, outermost first.
        std::vector<QWidget *> containers;
        for (QWidget *widget = cell->parentWidget(); widget && widget != view; widget = widget->parentWidget()) {
            if (qobject_cast<QSplitter *>(widget) || qobject_cast<QTabWidget *>(widget))
                containers.push_back(widget);
        }
        std::reverse(containers.begin(), containers.end());
        if (!replace && containers.size() == path->size()) {
            for (std::size_t depth = 0; depth < containers.size(); ++depth) {
                const auto *tabWidget = qobject_cast<QTabWidget *>(containers[depth]);
                if (!tabWidget || !tabWidget->tabBar()->isVisible())
                    continue;
                // The free space beside the tabs counts as the tab bar too.
                const QRect bar = rectIn(tabWidget->tabBar(), view);
                const QRect area(rectIn(tabWidget, view).left(), bar.top(), tabWidget->width(), bar.height());
                if (area.contains(pos)) {
                    const NodePath tabsPath(path->begin(), path->begin() + std::ptrdiff_t(depth));
                    return tabBarHit(view, tabWidget, tabsPath, pos);
                }
            }
        }

        const QRect rect = rectIn(cell, view);
        if (!rect.contains(pos))
            continue;
        if (replace) {
            DropHit hit;
            hit.target.path = *path;
            hit.target.zone.kind = DropZone::Kind::Replace;
            hit.highlight = rect;
            return hit;
        }
        return cellHit(*path, rect, pos);
    }
    return std::nullopt;
}

namespace {

// Forwards the events of the view to the overlay.
class ViewWatcher : public QObject
{
public:
    std::function<bool(QEvent *)> handler;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (handler && handler(event))
            return true;
        return QObject::eventFilter(watched, event);
    }
};

// Paints the highlight. The overlay itself has no paintEvent of its own.
class HitPainter : public QWidget
{
public:
    explicit HitPainter(QWidget *parent)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
    }

    std::optional<DropHit> hit;

protected:
    void paintEvent(QPaintEvent * /*event*/) override
    {
        if (!hit)
            return;
        QPainter painter(this);
        const QColor colour = palette().color(QPalette::Highlight);
        if (hit->target.zone.kind == DropZone::Kind::TabBar) {
            painter.fillRect(hit->highlight, colour);
            return;
        }
        // Replacing a cell's content covers the whole cell, more strongly.
        const bool replace = hit->target.zone.kind == DropZone::Kind::Replace;
        QColor fill = colour;
        fill.setAlpha(replace ? 120 : 70);
        painter.fillRect(hit->highlight, fill);
        painter.setPen(QPen(colour, replace ? 3 : 2));
        painter.drawRect(hit->highlight.adjusted(1, 1, -1, -1));
    }
};

} // namespace

struct DropOverlay::Private
{
    DropOverlay *q = nullptr;
    LayoutView *view = nullptr;
    HitPainter *painter = nullptr;
    ViewWatcher watcher;
    std::optional<DragPayload> payload;
    Acceptor acceptor;

    // Holding a drag over a tab opens it, so its content can take the drop.
    QTimer *tabTimer = nullptr;
    QPointer<QTabBar> hoveredBar;
    int hoveredTab = -1;

    void hoverTab(const QPoint &pos)
    {
        QTabBar *bar = nullptr;
        for (QWidget *widget = view->childAt(pos); widget && widget != view; widget = widget->parentWidget()) {
            if ((bar = qobject_cast<QTabBar *>(widget)))
                break;
        }
        const int tab = bar ? bar->tabAt(bar->mapFrom(view, pos)) : -1;
        if (bar == hoveredBar && tab == hoveredTab)
            return;
        hoveredBar = bar;
        hoveredTab = tab;
        if (bar && tab >= 0 && tab != bar->currentIndex())
            tabTimer->start();
        else
            tabTimer->stop();
    }

    void openHoveredTab()
    {
        if (hoveredBar && hoveredTab >= 0 && hoveredTab < hoveredBar->count())
            hoveredBar->setCurrentIndex(hoveredTab);
    }

    bool onViewEvent(QEvent *event);

    // Updates the hit for pos, and says whether a drop there is possible. A
    // zone where the drop would be refused is not highlighted. Ctrl makes the
    // drop replace the content of the cell under the cursor.
    bool track(const QPoint &pos, Qt::KeyboardModifiers modifiers)
    {
        // A locked dashboard refuses every drop, from the sidebar too.
        const bool replace = modifiers.testFlag(Qt::ControlModifier);
        if (view->editMode())
            hoverTab(pos);
        painter->hit = view->editMode() ? dropHitAt(view, pos, replace) : std::nullopt;
        if (painter->hit && payload && acceptor && !acceptor(painter->hit->target, *payload))
            painter->hit.reset();
        painter->update();
        return painter->hit.has_value();
    }

    void fit()
    {
        q->setGeometry(view->rect());
        painter->setGeometry(q->rect());
    }

    void finish()
    {
        tabTimer->stop();
        hoveredBar = nullptr;
        hoveredTab = -1;
        payload.reset();
        painter->hit.reset();
        q->hide();
    }
};

bool DropOverlay::Private::onViewEvent(QEvent *event)
{
    switch (event->type()) {
    case QEvent::Resize:
        fit();
        return false;

    case QEvent::DragEnter: {
        auto *drag = static_cast<QDragEnterEvent *>(event);
        payload = dragPayload(drag->mimeData());
        if (!payload) {
            drag->ignore();
            return true;
        }
        // The view under the cursor always takes the drag, even when locked,
        // and refuses it move by move instead. A drag refused here would go on
        // to the parent widgets, so a locked task cell would pass it to the
        // root dashboard around it, which decides by its own lock.
        if (!view->editMode()) {
            drag->acceptProposedAction();
            return true;
        }
        fit();
        q->raise();
        q->show();
        track(drag->position().toPoint(), drag->modifiers());
        // Accepting even outside a cell keeps the move events coming.
        drag->acceptProposedAction();
        return true;
    }

    case QEvent::DragMove: {
        auto *drag = static_cast<QDragMoveEvent *>(event);
        if (payload && track(drag->position().toPoint(), drag->modifiers()))
            drag->acceptProposedAction();
        else
            drag->ignore();
        return true;
    }

    case QEvent::DragLeave:
        finish();
        return true;

    case QEvent::Drop: {
        auto *drop = static_cast<QDropEvent *>(event);
        const std::optional<DragPayload> dropped = payload;
        const bool hit = dropped && track(drop->position().toPoint(), drop->modifiers());
        const std::optional<DropHit> target = painter->hit;
        finish();
        if (!hit || !target) {
            drop->ignore();
            return true;
        }
        drop->acceptProposedAction();
        emit q->dropped(target->target, *dropped);
        return true;
    }

    default:
        return false;
    }
}

DropOverlay::DropOverlay(LayoutView *view)
    : QWidget(view)
    , d(std::make_unique<Private>())
{
    d->q = this;
    d->view = view;
    d->painter = new HitPainter(this);
    d->tabTimer = new QTimer(this);
    d->tabTimer->setSingleShot(true);
    d->tabTimer->setInterval(tabOpenDelay);
    connect(d->tabTimer, &QTimer::timeout, this, [this] { d->openHoveredTab(); });
    // The overlay only paints. Drag events reach the view, which is watched
    // here, so Qt keeps finding the cells below as the widgets under the cursor.
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setFocusPolicy(Qt::NoFocus);
    hide();

    d->watcher.handler = [this](QEvent *event) { return d->onViewEvent(event); };
    view->setAcceptDrops(true);
    view->installEventFilter(&d->watcher);
}

DropOverlay::~DropOverlay()
{
    d->watcher.handler = nullptr;
    d->tabTimer->stop();
    d->tabTimer->disconnect(this);
}

void DropOverlay::setAcceptor(Acceptor acceptor)
{
    d->acceptor = std::move(acceptor);
}

} // namespace mirante
