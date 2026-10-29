#pragma once

#include <QByteArray>
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
    };
    Tabs openTabs(const QString &workspace) const;
    void setOpenTabs(const QString &workspace, const Tabs &tabs);

    // The main window. An invalid size means none was saved.
    QSize windowSize() const;
    void setWindowSize(const QSize &size);
    bool windowMaximized() const;
    void setWindowMaximized(bool maximized);
    QByteArray sidebarSplitterState() const;
    void setSidebarSplitterState(const QByteArray &state);

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
