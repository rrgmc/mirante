#pragma once

#include "api/ApiServices.h"
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

    // The tokens, servers and status cache. Call it before the first
    // workspace opens. Without them, as in most tests, cells show no status
    // and tokens typed into settings screens are dropped. They must outlive
    // the window. See the M5 plan, part 9.
    void setApiServices(const ApiServices &services);

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
    // workspace.mirante, and opens it. A workspace without root dashboards
    // gets one empty root dashboard, "Main", open as a tab and unlocked.
    // Returns false, and shows why, if it fails.
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
    // Moves the open workspace's folder to destination, which must not exist
    // yet or be empty, and opens it there. The device settings that name the
    // folder, such as the recent list and the open tabs, follow it. Web logins
    // and tokens are kept by profile id, not by folder, so they keep working.
    // The sample data cannot be moved. Returns false, and shows why, if the
    // workspace was not moved; it then stays open where it was.
    bool moveWorkspace(const QString &destination);

    void showAbout();
    // Opens the settings window, or brings it to the front: Settings… in the
    // application menu on macOS, Tools > Settings… elsewhere. Its changes
    // apply at once. See docs/ui.md#settings.
    void showSettings();
    // Opens the Modules window, or brings it to the front: Help > Modules….
    // It lists the modules of this build and their item types. See
    // docs/ui.md#modules-window.
    void showModules();

    // Navigation, used by the sidebar, the switcher and the shortcuts.
    // Without a dashboard id, these use the task's default dashboard.
    void openTaskPreview(const QString &taskId, const QString &dashboardId = QString());
    void openRootDashboard(const QString &id);
    void newRootDashboard();
    // Shows the list of all root dashboards, where closed ones are opened again
    // and any can be deleted.
    void showDashboardList();
    void focusItem(const QString &taskId, const QString &itemId);
    // Opens the item alone in the preview slot, like a task dashboard.
    void openItem(const QString &taskId, const QString &itemId);
    void revealInSidebar(const QString &id);
    void showQuickSwitcher();
    // Closes the preview or the current tab. Focus cannot be closed.
    void closeCurrentTab();
    // Locks or unlocks the current dashboard, as a click on its lock button
    // does. Focus and an item page have no lock, so nothing happens there.
    // The lock key, F2, calls it.
    void toggleCurrentLock();
    // Makes the Focus slot current.
    void openFocus();
    // The next (step 1) or previous (step -1) page, wrapping: the tabs of the
    // root tab bar in order, then Focus, then the preview if there is one.
    void switchPage(int step);
    // Ctrl+V with an image on the clipboard. Replaces the image of the image
    // item in the focused cell or the current item preview, through ImagePaste,
    // after the paste dialog. It never adds an item to a dashboard: on any
    // other cell the status bar points to "Paste Image…" in a task's Items
    // menu. See docs/decisions/0051-paste-images-from-the-items-menu.md.
    void pasteImage(const QImage &image);
    // An image dropped from another app on the cell of an image item:
    // replaces the item's image after the paste dialog, as Ctrl+V does, on a
    // locked dashboard too. See docs/decisions/0085-drop-images-from-other-apps.md.
    void dropImageOnItem(const QString &taskId, const QString &itemId, const QImage &image);
    // Runs the dialog of pasteImage() and says whether it was accepted.
    // The default calls exec(); tests work the dialog and return instead.
    void setExec(std::function<bool(QDialog &dialog)> exec);
    // Saves the open tabs of the open workspace on this device.
    void saveOpenTabs();
    // Full screen, as in web browsers: View > Full Screen, F11, or Ctrl+Cmd+F
    // on macOS. Leaving it brings back the maximized or normal window.
    void setFullScreen(bool on);

    // The tray icon and the global hotkey. main() calls this once; tests do not,
    // so they touch neither. It waits for a tray that is not there yet, as at
    // sign-in. See docs/ui.md#tray-and-notifications.
    void startTrayAndHotkey();
    // Started at sign-in: main() leaves the window hidden in the tray until it
    // is called. With no tray after the wait for one, the window shows
    // minimized. See docs/decisions/0105-start-at-sign-in.md.
    void startHidden();
    // Whether a tray exists to hide into. With one, the app no longer quits with
    // its last window, so it quits through QApplication::quit(). Tests set it
    // themselves.
    void setTrayPresent(bool present);
    // Brings the window back, restored if minimized, with the windows that were
    // hidden with it. An open modal dialog comes to the front with it.
    void showAndRaise();
    // Hides this window and the other open windows to the tray, or minimizes it
    // if there is no tray. The window's place and the open tabs are saved first.
    void hideToTray();
    // The global hotkey: hides the app if it is in front, else brings it back.
    void toggleVisible();
    // The app is quitting: the next close saves everything and really closes.
    // The application's quit event calls it, whatever asked for the quit.
    void prepareToQuit();

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
    // Saves the window's place and size, whether it is maximized, the sidebar
    // width and the open tabs on this device. The window opens at the saved
    // place only while that is still visible on a screen. With a tray and the
    // device setting on, the window hides instead, unless the app is quitting.
    void closeEvent(QCloseEvent *event) override;
    // Sees the application's quit event before Qt closes the windows.
    bool eventFilter(QObject *watched, QEvent *event) override;
    // Keeps View > Full Screen checked while the window is full screen.
    void showEvent(QShowEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
