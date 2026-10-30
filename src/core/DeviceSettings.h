#pragma once

#include <QByteArray>
#include <QRect>
#include <QSize>
#include <QString>
#include <QStringList>

#include <memory>

class QSettings;

namespace mirante {

// Typed access to the settings kept on this device, outside the data folder.
// See docs/sync.md#device-settings and the M3 plan, part 4. Every QSettings
// use in the app goes through this class. Implemented in DeviceSettings.cpp.
class DeviceSettings
{
public:
    // Without settings, uses the application's default QSettings. Tests pass
    // their own, which must outlive this object.
    explicit DeviceSettings(QSettings *settings = nullptr);
    // Keeps the settings in an INI file instead, made if missing. For test
    // runs that must not change this device's settings: the --settings option.
    explicit DeviceSettings(const QString &iniFile);
    ~DeviceSettings();
    DeviceSettings(const DeviceSettings &) = delete;
    DeviceSettings &operator=(const DeviceSettings &) = delete;

    // Workspaces: the last one opened, and the recent ones, most recent first.
    QString lastWorkspace() const;
    void setLastWorkspace(const QString &folder);
    QStringList recentWorkspaces() const;
    // Puts folder first, removing it from further down. Keeps ten.
    void addRecentWorkspace(const QString &folder);
    void removeRecentWorkspace(const QString &folder);
    void clearRecentWorkspaces();
    // After a workspace's folder moved from one path to another: the last
    // workspace, the recent list, the open tabs, the workspace id and project
    // folder paths inside the old folder take the new path. See docs/ui.md#menus.
    void moveWorkspace(const QString &from, const QString &to);

    // The workspace id this device last opened a workspace's folder with,
    // or empty. An empty id removes it. See
    // docs/decisions/0062-logins-follow-a-new-workspace-id.md.
    QString workspaceId(const QString &workspace) const;
    void setWorkspaceId(const QString &workspace, const QString &id);

    // The local path of a project folder, by the folder's id. Empty if not set.
    // See docs/decisions/0010-folder-identity.md.
    QString folderPath(const QString &folderId) const;
    // An empty path removes the mapping.
    void setFolderPath(const QString &folderId, const QString &path);

    // The open root tabs of one workspace, keyed by its folder. The preview
    // tab is not kept.
    struct Tabs
    {
        QStringList ids;            // root dashboard ids, and "focus", in tab order
        QString current;            // the id of the current tab
        bool saved = false;         // false if nothing was saved for this workspace yet
        QStringList detached;       // root dashboards shown in their own windows; not in ids
    };
    Tabs openTabs(const QString &workspace) const;
    void setOpenTabs(const QString &workspace, const Tabs &tabs);

    // The main window. An invalid size means none was saved.
    QSize windowSize() const;
    void setWindowSize(const QSize &size);
    // The main window's client area when it is neither maximized nor full
    // screen, in screen coordinates: its position and size. An invalid
    // rectangle means none was saved, as by versions before it was kept.
    QRect windowGeometry() const;
    void setWindowGeometry(const QRect &geometry);
    bool windowMaximized() const;
    void setWindowMaximized(bool maximized);
    QByteArray sidebarSplitterState() const;
    void setSidebarSplitterState(const QByteArray &state);

    // The window of a detached root dashboard, as QWidget::saveGeometry()
    // returns it. Empty if none was saved. Root dashboard ids are UUIDs, so one
    // map serves every workspace. See docs/decisions/0024-detach-root-dashboard-tabs.md.
    QByteArray detachedGeometry(const QString &dashboardId) const;
    void setDetachedGeometry(const QString &dashboardId, const QByteArray &geometry);

    // Whether the sidebar tints the row of what the dashboard cell under the
    // mouse shows. On if not set. Set in the settings window; see
    // docs/ui.md#settings.
    bool sidebarHoverTint() const;
    void setSidebarHoverTint(bool on);

    // The number of hidden web views kept loaded. 8 if not set.
    // See docs/web-views.md#memory-policy.
    int hiddenViewLimit() const;
    void setHiddenViewLimit(int limit);

    // How often the status cache fetches a due item, in seconds: 120 if not
    // set, kept between 30 and 3600. Set in the settings window. See
    // docs/integrations.md#polling-and-caching.
    int pollInterval() const;
    void setPollInterval(int seconds);

    // The keys of the attention reasons the user has seen on an item, in a
    // workspace. Empty if none, and empty too for an item never recorded:
    // hasSeenAttention() tells the two apart. See
    // docs/decisions/0061-attention-from-status-of-any-rendering.md.
    QStringList seenAttention(const QString &workspaceId, const QString &itemId) const;
    bool hasSeenAttention(const QString &workspaceId, const QString &itemId) const;
    void setSeenAttention(const QString &workspaceId, const QString &itemId, const QStringList &keys);
    void removeSeenAttention(const QString &workspaceId, const QString &itemId);

    // Whether Focus lists the tasks that need attention first. Off if not
    // set. See docs/ui.md#focus-view.
    bool focusAttentionFirst() const;
    void setFocusAttentionFirst(bool on);

    // The global hotkey that shows or hides the window, as portable key
    // sequence text. Alt+Shift+D if not set (Control+Shift+D on macOS); empty
    // turns it off. See docs/ui.md#keyboard.
    QString globalHotkey() const;
    void setGlobalHotkey(const QString &sequence);

    // Whether closing the main window hides it to the tray instead of quitting.
    // On if not set. See docs/ui.md#tray-and-notifications.
    bool closeToTray() const;
    void setCloseToTray(bool on);

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
