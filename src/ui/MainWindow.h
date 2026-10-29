#pragma once

#include "core/Store.h"

#include <QMainWindow>

class QImage;

#include <memory>
#include <vector>

namespace mirante {

class DeviceSettings;
class EntityActions;
class ItemTypeRegistry;
class LayoutView;
class Model;
class RootTabBar;
class Shortcuts;

// The sidebar on the left; on the right the root tab bar over the dashboards,
// a warning bar for the Store's problems and the read-only note, and the
// conflict bar. It owns the LayoutServices objects and creates Shortcuts,
// QuickSwitcher, EntityActions and ImagePaste. See the M2 plan, part 5, and
// the M3 plan, part 7. Implemented in MainWindow.cpp.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    // The Store loads and saves the workspaces the window opens. The settings
    // keep the window's size, the workspaces and their open tabs.
    MainWindow(Model &model, const ItemTypeRegistry &types, Store &store, DeviceSettings &settings,
               QWidget *parent = nullptr);
    ~MainWindow() override;

    // Shows the problems in the warning bar, with the Store's own problems,
    // or hides it if there are none. Opening a workspace clears them.
    void setLoadProblems(const std::vector<LoadProblem> &problems);

    // A workspace is a data folder with a workspace.mirante file at its top.
    // Opening one saves the open tabs of the previous one, opens it through
    // the Store, restores its open tabs, remembers it on this device as the
    // last one, and puts it first in the recent list. path is the folder, or
    // the path of its workspace.mirante file. A folder without the file is not
    // opened; the warning bar says why.
    void openWorkspace(const QString &path);
    // Makes folder a workspace with the given name, by writing its
    // workspace.mirante, and opens it. Returns false, and shows why, if it fails.
    bool createWorkspace(const QString &folder, const QString &name);
    // The sample data compiled into the app, used when there is no workspace.
    // It is read-only: the warning bar says that changes are not saved.
    void openSampleData();
    // Opens the workspace again through the Store, or the sample data.
    void reloadWorkspace();
    // The open workspace's folder, or empty for the sample data.
    QString workspace() const;
    // The recent workspaces, most recent first.
    QStringList recentWorkspaces() const;
    // Renames the open workspace through the model; the Store writes its
    // workspace.mirante. The sample data cannot be renamed. Returns false if
    // it is refused.
    bool renameWorkspace(const QString &name);

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
    // Opens the Focus tab at the start of the tab bar, or switches to it.
    void openFocus();
    // Pastes an image onto the dashboard or into the item of the current tab,
    // through ImagePaste, and shows its message in the status bar. Ctrl+V calls
    // it when the clipboard holds an image.
    void pasteImage(const QImage &image);
    // Saves the open tabs of the open workspace on this device.
    void saveOpenTabs();

    RootTabBar *rootTabBar() const;
    LayoutView *currentLayoutView() const;
    QWidget *contentArea() const;     // the area a maximised cell fills
    Shortcuts *shortcuts() const;
    EntityActions *entityActions() const;

protected:
    // Saves the window's size, whether it is maximized, the sidebar width and
    // the open tabs on this device. The window always opens centred.
    void closeEvent(QCloseEvent *event) override;

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
