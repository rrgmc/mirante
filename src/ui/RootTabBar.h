#pragma once

#include <QTabBar>

#include <functional>
#include <memory>

class QStyleOptionTab;

namespace mirante {

namespace tabbar {

// Draws the tabs of a bar as QTabBar does. The tab at italicIndex, if any, has
// an italic title. The current tab is drawn as selected only if
// selectionShown; it is drawn last so that it overlaps its neighbours. init
// fills a tab's style option, since QTabBar::initStyleOption is protected.
void paintTabs(QTabBar &bar, const QRect &dirty, const std::function<void(QStyleOptionTab *, int)> &init,
               int italicIndex, bool selectionShown);

// Shows or hides the current tab's selection. The window keeps one current
// page across the root tab bar and the slot bar (decision 0048), so one of the
// two bars shows no selection. AppStyle reads the flag to hide the current
// tab's close button too.
void setSelectionShown(QTabBar &bar, bool shown);
bool selectionShown(const QTabBar &bar);

} // namespace tabbar

// The tab bar of root dashboards. The + button asks for a new root dashboard.
// Dragging a tab along the bar moves it; QTabBar::tabMoved() reports it.
// A middle click on a tab asks to close it, and the context menu holds "New dashboard", "Close tab" and "All
// dashboards…". A tab whose tab data is true is a root dashboard, so its menu
// also holds "Rename dashboard…" and "Detach into Window". The preview and
// Focus are not here: they are in the SlotTabBar on the right of the row.
// Implemented in RootTabBar.cpp.
class RootTabBar : public QTabBar
{
    Q_OBJECT

public:
    explicit RootTabBar(QWidget *parent = nullptr);
    ~RootTabBar() override;

    // False while a slot of the SlotTabBar is current: the bar keeps its
    // current index but draws no tab as selected.
    void setSelectionShown(bool shown);
    bool selectionShown() const;

signals:
    // A left click on a tab, even the current one. While a slot is current,
    // a click on the current root tab makes it current again.
    void tabPressed(int index);
    void newDashboardRequested();
    void dashboardListRequested();
    void renameRequested(int index);
    void detachRequested(int index);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
};

} // namespace mirante
