#include "ui/RootTabBar.h"

#include <QContextMenuEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QStyleOptionTab>
#include <QStylePainter>

namespace mirante {

namespace tabbar {

namespace {
// The dynamic property AppStyle reads; see AppStyle.cpp.
const char *const selectionHiddenProperty = "miranteSelectionHidden";
} // namespace

void paintTabs(QTabBar &bar, const QRect &dirty, const std::function<void(QStyleOptionTab *, int)> &init,
               int italicIndex, bool selectionShown)
{
    QStylePainter painter(&bar);
    const QFont normal = bar.font();
    QFont italic = normal;
    italic.setItalic(true);

    auto drawTab = [&](int index) {
        QStyleOptionTab option;
        init(&option, index);
        if (!option.rect.intersects(dirty))
            return;
        if (!selectionShown)
            option.state &= ~QStyle::State_Selected;
        painter.drawControl(QStyle::CE_TabBarTabShape, option);
        painter.setFont(index == italicIndex ? italic : normal);
        painter.drawControl(QStyle::CE_TabBarTabLabel, option);
        painter.setFont(normal);
    };

    const int selected = bar.currentIndex();
    for (int i = 0; i < bar.count(); ++i) {
        if (i != selected && bar.isTabVisible(i))
            drawTab(i);
    }
    if (selected >= 0 && bar.isTabVisible(selected))
        drawTab(selected);
}

void setSelectionShown(QTabBar &bar, bool shown)
{
    if (selectionShown(bar) == shown)
        return;
    bar.setProperty(selectionHiddenProperty, !shown);
    bar.update();
    // The close buttons paint the current tab's state too.
    for (QWidget *child : bar.findChildren<QWidget *>(Qt::FindDirectChildrenOnly))
        child->update();
}

bool selectionShown(const QTabBar &bar) { return !bar.property(selectionHiddenProperty).toBool(); }

} // namespace tabbar

RootTabBar::RootTabBar(QWidget *parent)
    : QTabBar(parent)
{
    setTabsClosable(true);
    setDrawBase(false);
    setExpanding(false);
    setUsesScrollButtons(true);
    setElideMode(Qt::ElideRight);
    setMovable(true);
}

RootTabBar::~RootTabBar() = default;

void RootTabBar::setSelectionShown(bool shown) { tabbar::setSelectionShown(*this, shown); }

bool RootTabBar::selectionShown() const { return tabbar::selectionShown(*this); }

void RootTabBar::paintEvent(QPaintEvent *event)
{
    // Only QTabBar's own painting draws a tab being dragged. A drag starts with
    // a press, which makes the tab current, so the selection is shown by then.
    if (selectionShown()) {
        QTabBar::paintEvent(event);
        return;
    }
    tabbar::paintTabs(
        *this, event->rect(), [this](QStyleOptionTab *option, int index) { initStyleOption(option, index); }, -1,
        selectionShown());
}

void RootTabBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        const int index = tabAt(event->position().toPoint());
        if (index >= 0)
            emit tabPressed(index);
    }
    QTabBar::mousePressEvent(event);
}

// A middle click on a tab closes it, as in web browsers.
void RootTabBar::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton) {
        const int index = tabAt(event->position().toPoint());
        if (index >= 0) {
            emit tabCloseRequested(index);
            return;
        }
    }
    QTabBar::mouseReleaseEvent(event);
}

void RootTabBar::contextMenuEvent(QContextMenuEvent *event)
{
    const int index = tabAt(event->pos());
    QMenu menu(this);
    QAction *add = menu.addAction(tr("New dashboard"));
    QAction *close = index >= 0 ? menu.addAction(tr("Close tab")) : nullptr;
    // Only root dashboard tabs can be detached. Their tab data says so.
    const bool root = index >= 0 && tabData(index).toBool();
    QAction *rename = root ? menu.addAction(tr("Rename dashboard…")) : nullptr;
    QAction *detach = root ? menu.addAction(tr("Detach into Window")) : nullptr;
    menu.addSeparator();
    QAction *list = menu.addAction(tr("All dashboards…"));
    QAction *chosen = menu.exec(event->globalPos());
    if (chosen == add)
        emit newDashboardRequested();
    else if (chosen && chosen == close)
        emit tabCloseRequested(index);
    else if (chosen && chosen == rename)
        emit renameRequested(index);
    else if (chosen && chosen == detach)
        emit detachRequested(index);
    else if (chosen == list)
        emit dashboardListRequested();
}

} // namespace mirante
