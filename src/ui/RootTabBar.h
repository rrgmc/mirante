#pragma once

#include <QTabBar>

#include <memory>

namespace mirante {

// The tab bar of root dashboards. The preview tab has an italic title and a
// pin button. The + button asks for a new root dashboard. A middle click on a
// tab asks to close it, and the context menu holds "New dashboard", "Close tab"
// and "All dashboards…". A tab whose tab data is true is a root dashboard, so
// its menu also holds "Rename dashboard…" and "Detach into Window".
// Implemented in RootTabBar.cpp.
class RootTabBar : public QTabBar
{
    Q_OBJECT

public:
    explicit RootTabBar(QWidget *parent = nullptr);
    ~RootTabBar() override;

    // The index of the preview tab, or -1 if there is none.
    void setPreviewIndex(int index);
    int previewIndex() const;

signals:
    void pinRequested(int index);
    void newDashboardRequested();
    void dashboardListRequested();
    void renameRequested(int index);
    void detachRequested(int index);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
