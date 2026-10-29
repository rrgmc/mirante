#pragma once

#include "core/Store.h"

#include <QMainWindow>

class QDialog;
class QImage;

#include <functional>
#include <memory>
#include <vector>

namespace mirante {

class DeviceSettings;
class EntityActions;
class ItemTypeRegistry;
class LayoutView;
class Model;
class ProfileManager;
class RootTabBar;
class Shortcuts;
class SlotTabBar;
class ViewPool;

// The sidebar on the left; on the right the tab row over the dashboards, a
// warning bar for the Store's problems and the read-only note, and the
// conflict bar. The tab row holds the root tab bar on the left and the Focus
// and preview slots on the right (decision 0048); one page is current across
// both. It owns the LayoutServices objects and creates Shortcuts,
// QuickSwitcher, EntityActions and ImagePaste. See the M2 plan, part 5, and
// the M3 plan, part 7. Implemented in MainWindow.cpp.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    // The Store loads and saves the workspaces the window opens. The settings
    // keep the window's size, the workspaces and their open tabs. The profiles
    // and the pool give item cells their views; both must outlive the window.
    // Without profiles, item cells keep their placeholders, which tests use.
    MainWindow(Model &model, const ItemTypeRegistry &types, Store &store, DeviceSettings &settings,
               ProfileManager *profiles = nullptr, ViewPool *pool = nullptr, QWidget *parent = nullptr);
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
    // Opens the item alone in the preview slot, like a task dashboard, or
    // switches to its tab if it has one.
    void openItem(const QString &taskId, const QString &itemId);
    void revealInSidebar(const QString &id);
    void showQuickSwitcher();
    // Closes the preview or the current tab. Focus cannot be closed.
    void closeCurrentTab();
    // Makes the Focus slot current.
    void openFocus();
    // The next (step 1) or previous (step -1) page, wrapping: the tabs of the
    // root tab bar in order, then Focus, then the preview if there is one.
    void switchPage(int step);
    // Ctrl+V with an image on the clipboard. Replaces the image of the image
    // item in the focused cell or the current item tab, through ImagePaste,
    // after the paste dialog. It never adds an item to a dashboard: on any
    // other cell the status bar points to "Paste Image…" in a task's Items
    // menu. See docs/decisions/0051-paste-images-from-the-items-menu.md.
    void pasteImage(const QImage &image);
    // Runs the dialog of pasteImage() and says whether it was accepted.
    // The default calls exec(); tests work the dialog and return instead.
    void setExec(std::function<bool(QDialog &dialog)> exec);
    // Saves the open tabs of the open workspace on this device.
    void saveOpenTabs();
    // Full screen, as in web browsers: View > Full Screen, F11, or Ctrl+Cmd+F
    // on macOS. Leaving it brings back the maximized or normal window.
    void setFullScreen(bool on);

    RootTabBar *rootTabBar() const;
    SlotTabBar *slotTabBar() const;
    LayoutView *currentLayoutView() const;
    QWidget *contentArea() const;     // the area a maximised cell fills
    Shortcuts *shortcuts() const;
    EntityActions *entityActions() const;

signals:
    // The page in the content area changed: another tab or slot is current.
    void currentPageChanged();

protected:
    // Saves the window's size, whether it is maximized, the sidebar width and
    // the open tabs on this device. The window always opens centred.
    void closeEvent(QCloseEvent *event) override;
    // Keeps View > Full Screen checked while the window is full screen.
    void changeEvent(QEvent *event) override;

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
