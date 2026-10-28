#include "layout/DropOverlay.h"

#include "layout/DropOverlayTarget.h"
#include "layout/LayoutView.h"

#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QPainter>
#include <QSplitter>
#include <QTabBar>
#include <QTabWidget>

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

std::optional<DropHit> dropHitAt(const LayoutView *view, const QPoint &pos)
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
        if (containers.size() == path->size()) {
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
        if (rect.contains(pos))
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
        QColor fill = colour;
        fill.setAlpha(70);
        painter.fillRect(hit->highlight, fill);
        painter.setPen(QPen(colour, 2));
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

    bool onViewEvent(QEvent *event);

    // Updates the hit for pos, and says whether a drop there is possible. A
    // zone where the drop would be refused is not highlighted.
    bool track(const QPoint &pos)
    {
        painter->hit = dropHitAt(view, pos);
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
        // Moving a cell needs edit mode. A drag from the sidebar works on a
        // locked dashboard too.
        if (!payload || (payload->sourceDashboard && !view->editMode())) {
            payload.reset();
            drag->ignore();
            return true;
        }
        fit();
        q->raise();
        q->show();
        track(drag->position().toPoint());
        // Accepting even outside a cell keeps the move events coming.
        drag->acceptProposedAction();
        return true;
    }

    case QEvent::DragMove: {
        auto *drag = static_cast<QDragMoveEvent *>(event);
        if (payload && track(drag->position().toPoint()))
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
        const bool hit = dropped && track(drop->position().toPoint());
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
}

void DropOverlay::setAcceptor(Acceptor acceptor)
{
    d->acceptor = std::move(acceptor);
}

} // namespace mirante
