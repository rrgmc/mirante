#pragma once

#include "core/Store.h"

#include <QMainWindow>

#include <memory>
#include <vector>

namespace mirante {

class ItemTypeRegistry;
class LayoutView;
class Model;
class RootTabBar;
class Shortcuts;

// The sidebar on the left; on the right the root tab bar over the dashboards,
// and a warning bar for load problems. It owns the LayoutServices objects and
// creates Shortcuts and QuickSwitcher. See the M2 plan, part 5.
// Implemented in MainWindow.cpp.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(Model &model, const ItemTypeRegistry &types, QWidget *parent = nullptr);
    ~MainWindow() override;

    // Shows the problems in the warning bar, or hides it if there are none.
    void setLoadProblems(const std::vector<LoadProblem> &problems);

    // A workspace is a data folder. Opening one loads it, remembers it on this
    // device as the last one, and puts it first in the recent list. In M2
    // nothing is written back to it.
    void openWorkspace(const QString &folder);
    // The sample data compiled into the app, used when there is no workspace.
    void openSampleData();
    // The open workspace's folder, or empty for the sample data.
    QString workspace() const;
    // The recent workspaces, most recent first.
    QStringList recentWorkspaces() const;

    void showAbout();

    // Navigation, used by the sidebar, the switcher and the shortcuts.
    // Without a dashboard id, these use the task's default dashboard.
    void openTaskPreview(const QString &taskId, const QString &dashboardId = QString());
    void pinTask(const QString &taskId, const QString &dashboardId = QString());
    void openRootDashboard(const QString &id);
    void newRootDashboard();
    // Shows the list of all root dashboards, where closed ones are opened again
    // and any can be deleted.
    void showDashboardList();
    void focusItem(const QString &taskId, const QString &itemId);
    // Opens the item alone in a tab, the preview tab like a task dashboard,
    // or switches to its tab if it has one.
    void openItem(const QString &taskId, const QString &itemId);
    void revealInSidebar(const QString &id);
    void showQuickSwitcher();
    void closeCurrentTab();     // the preview tab or the current root tab

    RootTabBar *rootTabBar() const;
    LayoutView *currentLayoutView() const;
    QWidget *contentArea() const;     // the area a maximised cell fills
    Shortcuts *shortcuts() const;

protected:
    // Saves the window's size, whether it is maximized, and the sidebar width
    // on this device. The window always opens centred.
    void closeEvent(QCloseEvent *event) override;

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
