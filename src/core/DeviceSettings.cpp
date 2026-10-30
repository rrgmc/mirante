#include "core/DeviceSettings.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSettings>
#include <QSysInfo>

#include <algorithm>

namespace mirante {

namespace {

// The keys M2 already used, kept so existing settings carry over.
const QString lastWorkspaceKey = QStringLiteral("workspace/last");
const QString recentWorkspacesKey = QStringLiteral("workspace/recent");
const QString windowSizeKey = QStringLiteral("window/size");
const QString windowGeometryKey = QStringLiteral("window/geometry");
const QString windowMaximizedKey = QStringLiteral("window/maximized");
const QString windowSplitterKey = QStringLiteral("window/splitter");
const QString sidebarHoverTintKey = QStringLiteral("sidebar/hoverTint");
const QString hiddenViewLimitKey = QStringLiteral("webViews/hiddenLimit");
const QString pollIntervalKey = QStringLiteral("status/pollInterval");
const QString focusAttentionFirstKey = QStringLiteral("focus/attentionFirst");
const QString globalHotkeyKey = QStringLiteral("window/globalHotkey");
const QString closeToTrayKey = QStringLiteral("window/closeToTray");
const QString terminalCommandKey = QStringLiteral("terminal/command");
const QString deviceNameKey = QStringLiteral("device/name");

constexpr int defaultHiddenViewLimit = 8;

constexpr int defaultPollInterval = 120;
constexpr int minPollInterval = 30;
constexpr int maxPollInterval = 3600;

constexpr int maxRecent = 10;

QString folderKey(const QString &folderId)
{
    return QStringLiteral("folders/") + folderId;
}

// A module's device setting. Module ids and field keys are plain names.
QString moduleSettingKey(const QString &moduleId, const QString &key)
{
    return QStringLiteral("modules/") + moduleId + QLatin1Char('/') + key;
}

QString detachedKey(const QString &dashboardId)
{
    return QStringLiteral("detached/") + dashboardId;
}

// The seen attention keys of one item. Workspace and item ids are UUIDs, so
// they are safe in a key.
QString seenAttentionKey(const QString &workspaceId, const QString &itemId)
{
    return QStringLiteral("seenAttention/") + workspaceId + QLatin1Char('/') + itemId;
}

QString cleanAbsolute(const QString &path)
{
    return path.isEmpty() ? QString() : QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

// A key for one workspace under prefix. A path can hold characters a
// settings key cannot, so the key is named by a hash of the cleaned absolute
// path.
QString workspaceKey(const QString &prefix, const QString &workspace)
{
    const QString path = QDir::cleanPath(QFileInfo(workspace).absoluteFilePath());
    const QByteArray hash = QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Sha1).toHex();
    return prefix + QString::fromLatin1(hash);
}

// The group holding one workspace's open tabs.
QString tabsGroup(const QString &workspace) { return workspaceKey(QStringLiteral("tabs/"), workspace); }

// The id this device last opened a workspace with.
QString workspaceIdKey(const QString &workspace) { return workspaceKey(QStringLiteral("workspaceIds/"), workspace); }

} // namespace

struct DeviceSettings::Private
{
    std::unique_ptr<QSettings> owned;   // set when no QSettings were given
    QSettings *settings = nullptr;
};

DeviceSettings::DeviceSettings(QSettings *settings)
    : d(std::make_unique<Private>())
{
    if (!settings) {
        d->owned = std::make_unique<QSettings>();
        settings = d->owned.get();
    }
    d->settings = settings;
}

DeviceSettings::DeviceSettings(const QString &iniFile)
    : d(std::make_unique<Private>())
{
    d->owned = std::make_unique<QSettings>(iniFile, QSettings::IniFormat);
    d->settings = d->owned.get();
}

DeviceSettings::~DeviceSettings() = default;

QString DeviceSettings::lastWorkspace() const
{
    return d->settings->value(lastWorkspaceKey).toString();
}

void DeviceSettings::setLastWorkspace(const QString &folder)
{
    d->settings->setValue(lastWorkspaceKey, folder);
}

QStringList DeviceSettings::recentWorkspaces() const
{
    return d->settings->value(recentWorkspacesKey).toStringList();
}

void DeviceSettings::addRecentWorkspace(const QString &folder)
{
    if (folder.isEmpty())
        return;
    QStringList list = recentWorkspaces();
    list.removeAll(folder);
    list.prepend(folder);
    while (list.size() > maxRecent)
        list.removeLast();
    d->settings->setValue(recentWorkspacesKey, list);
}

void DeviceSettings::removeRecentWorkspace(const QString &folder)
{
    QStringList list = recentWorkspaces();
    list.removeAll(folder);
    d->settings->setValue(recentWorkspacesKey, list);
}

void DeviceSettings::clearRecentWorkspaces()
{
    d->settings->remove(recentWorkspacesKey);
}

void DeviceSettings::moveWorkspace(const QString &from, const QString &to)
{
    const QString oldPath = cleanAbsolute(from);
    const QString newPath = cleanAbsolute(to);
    if (oldPath.isEmpty() || newPath.isEmpty() || oldPath == newPath)
        return;
    // A path is the old folder or inside it; the result is the same place
    // under the new folder, or empty.
    const auto moved = [&](const QString &path) -> QString {
        const QString clean = cleanAbsolute(path);
        if (clean == oldPath)
            return newPath;
        if (clean.startsWith(oldPath + QLatin1Char('/')))
            return newPath + clean.mid(oldPath.size());
        return {};
    };

    QSettings &settings = *d->settings;
    if (!moved(lastWorkspace()).isEmpty())
        setLastWorkspace(newPath);

    QStringList recent;
    for (const QString &folder : recentWorkspaces()) {
        const QString path = moved(folder) == newPath ? newPath : folder;
        if (!recent.contains(path))
            recent << path;
    }
    if (!recent.isEmpty())
        settings.setValue(recentWorkspacesKey, recent);

    const Tabs tabs = openTabs(oldPath);
    if (tabs.saved) {
        setOpenTabs(newPath, tabs);
        settings.remove(tabsGroup(oldPath));
    }

    const QString id = workspaceId(oldPath);
    if (!id.isEmpty()) {
        setWorkspaceId(newPath, id);
        setWorkspaceId(oldPath, QString());
    }

    // Project folders kept inside the workspace's folder moved with it.
    settings.beginGroup(QStringLiteral("folders"));
    const QStringList folderIds = settings.childKeys();
    settings.endGroup();
    for (const QString &folderId : folderIds) {
        const QString path = moved(folderPath(folderId));
        if (!path.isEmpty())
            setFolderPath(folderId, path);
    }
}

QString DeviceSettings::workspaceId(const QString &workspace) const
{
    if (workspace.isEmpty())
        return {};
    return d->settings->value(workspaceIdKey(workspace)).toString();
}

void DeviceSettings::setWorkspaceId(const QString &workspace, const QString &id)
{
    if (workspace.isEmpty())
        return;
    if (id.isEmpty())
        d->settings->remove(workspaceIdKey(workspace));
    else
        d->settings->setValue(workspaceIdKey(workspace), id);
}

QString DeviceSettings::folderPath(const QString &folderId) const
{
    if (folderId.isEmpty())
        return {};
    return d->settings->value(folderKey(folderId)).toString();
}

void DeviceSettings::setFolderPath(const QString &folderId, const QString &path)
{
    if (folderId.isEmpty())
        return;
    if (path.isEmpty())
        d->settings->remove(folderKey(folderId));
    else
        d->settings->setValue(folderKey(folderId), path);
}

DeviceSettings::Tabs DeviceSettings::openTabs(const QString &workspace) const
{
    Tabs tabs;
    QSettings &settings = *d->settings;
    settings.beginGroup(tabsGroup(workspace));
    tabs.saved = settings.contains(QStringLiteral("ids"));
    if (tabs.saved) {
        tabs.ids = settings.value(QStringLiteral("ids")).toStringList();
        tabs.current = settings.value(QStringLiteral("current")).toString();
        tabs.detached = settings.value(QStringLiteral("detached")).toStringList();
    }
    settings.endGroup();
    return tabs;
}

void DeviceSettings::setOpenTabs(const QString &workspace, const Tabs &tabs)
{
    QSettings &settings = *d->settings;
    settings.beginGroup(tabsGroup(workspace));
    // Clearing the group also drops the preview keys earlier versions wrote.
    settings.remove(QString());
    // The path itself, so a person reading the settings can tell the groups apart.
    settings.setValue(QStringLiteral("path"), QDir::cleanPath(QFileInfo(workspace).absoluteFilePath()));
    settings.setValue(QStringLiteral("ids"), tabs.ids);
    settings.setValue(QStringLiteral("current"), tabs.current);
    settings.setValue(QStringLiteral("detached"), tabs.detached);
    settings.endGroup();
}

QSize DeviceSettings::windowSize() const
{
    return d->settings->value(windowSizeKey).toSize();
}

void DeviceSettings::setWindowSize(const QSize &size)
{
    d->settings->setValue(windowSizeKey, size);
}

QRect DeviceSettings::windowGeometry() const
{
    return d->settings->value(windowGeometryKey).toRect();
}

void DeviceSettings::setWindowGeometry(const QRect &geometry)
{
    d->settings->setValue(windowGeometryKey, geometry);
}

bool DeviceSettings::windowMaximized() const
{
    return d->settings->value(windowMaximizedKey).toBool();
}

void DeviceSettings::setWindowMaximized(bool maximized)
{
    d->settings->setValue(windowMaximizedKey, maximized);
}

QByteArray DeviceSettings::sidebarSplitterState() const
{
    return d->settings->value(windowSplitterKey).toByteArray();
}

void DeviceSettings::setSidebarSplitterState(const QByteArray &state)
{
    d->settings->setValue(windowSplitterKey, state);
}

QByteArray DeviceSettings::detachedGeometry(const QString &dashboardId) const
{
    if (dashboardId.isEmpty())
        return {};
    return d->settings->value(detachedKey(dashboardId)).toByteArray();
}

// An empty geometry removes the entry.
void DeviceSettings::setDetachedGeometry(const QString &dashboardId, const QByteArray &geometry)
{
    if (dashboardId.isEmpty())
        return;
    if (geometry.isEmpty())
        d->settings->remove(detachedKey(dashboardId));
    else
        d->settings->setValue(detachedKey(dashboardId), geometry);
}

bool DeviceSettings::sidebarHoverTint() const
{
    return d->settings->value(sidebarHoverTintKey, true).toBool();
}

void DeviceSettings::setSidebarHoverTint(bool on)
{
    d->settings->setValue(sidebarHoverTintKey, on);
}

// The value is edited by hand in the settings file, so anything that is not a
// positive whole number counts as not set.
int DeviceSettings::hiddenViewLimit() const
{
    bool ok = false;
    const int limit = d->settings->value(hiddenViewLimitKey).toInt(&ok);
    return ok && limit > 0 ? limit : defaultHiddenViewLimit;
}

void DeviceSettings::setHiddenViewLimit(int limit)
{
    d->settings->setValue(hiddenViewLimitKey, limit);
}

// Out of range values are kept in range rather than refused, and anything
// that is not a whole number counts as not set, as the file can be edited by
// hand.
int DeviceSettings::pollInterval() const
{
    bool ok = false;
    const int seconds = d->settings->value(pollIntervalKey).toInt(&ok);
    if (!ok)
        return defaultPollInterval;
    return std::clamp(seconds, minPollInterval, maxPollInterval);
}

void DeviceSettings::setPollInterval(int seconds)
{
    d->settings->setValue(pollIntervalKey, std::clamp(seconds, minPollInterval, maxPollInterval));
}

QStringList DeviceSettings::seenAttention(const QString &workspaceId, const QString &itemId) const
{
    if (workspaceId.isEmpty() || itemId.isEmpty())
        return {};
    return d->settings->value(seenAttentionKey(workspaceId, itemId)).toStringList();
}

bool DeviceSettings::hasSeenAttention(const QString &workspaceId, const QString &itemId) const
{
    if (workspaceId.isEmpty() || itemId.isEmpty())
        return false;
    return d->settings->contains(seenAttentionKey(workspaceId, itemId));
}

// An empty list is kept as a record: the item was looked at and nothing in it
// clears on view.
void DeviceSettings::setSeenAttention(const QString &workspaceId, const QString &itemId, const QStringList &keys)
{
    if (workspaceId.isEmpty() || itemId.isEmpty())
        return;
    d->settings->setValue(seenAttentionKey(workspaceId, itemId), keys);
}

void DeviceSettings::removeSeenAttention(const QString &workspaceId, const QString &itemId)
{
    if (workspaceId.isEmpty() || itemId.isEmpty())
        return;
    d->settings->remove(seenAttentionKey(workspaceId, itemId));
}

bool DeviceSettings::focusAttentionFirst() const
{
    return d->settings->value(focusAttentionFirstKey, false).toBool();
}

void DeviceSettings::setFocusAttentionFirst(bool on)
{
    d->settings->setValue(focusAttentionFirstKey, on);
}

QString DeviceSettings::globalHotkey() const
{
#if defined(Q_OS_MACOS)
    // Qt's Meta is the Control key on macOS. macOS 15 refuses hotkeys that use
    // only Option and Shift.
    const QString standard = QStringLiteral("Meta+Shift+D");
#else
    const QString standard = QStringLiteral("Alt+Shift+D");
#endif
    return d->settings->value(globalHotkeyKey, standard).toString();
}

void DeviceSettings::setGlobalHotkey(const QString &sequence)
{
    d->settings->setValue(globalHotkeyKey, sequence);
}

bool DeviceSettings::closeToTray() const
{
    return d->settings->value(closeToTrayKey, true).toBool();
}

void DeviceSettings::setCloseToTray(bool on)
{
    d->settings->setValue(closeToTrayKey, on);
}

QString DeviceSettings::terminalCommand() const
{
    return d->settings->value(terminalCommandKey).toString();
}

void DeviceSettings::setTerminalCommand(const QString &command)
{
    if (command.trimmed().isEmpty())
        d->settings->remove(terminalCommandKey);
    else
        d->settings->setValue(terminalCommandKey, command);
}

QString DeviceSettings::deviceName() const
{
    const QString name = d->settings->value(deviceNameKey).toString().trimmed();
    return name.isEmpty() ? QSysInfo::machineHostName() : name;
}

void DeviceSettings::setDeviceName(const QString &name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        d->settings->remove(deviceNameKey);
    else
        d->settings->setValue(deviceNameKey, trimmed);
}

// The value is kept as JSON text, such as "code" with its quotes, or true, so
// it reads back with its type: an INI file would otherwise give every value
// back as a string. Text that is not JSON, as typed into the file by hand,
// reads as a string.
QJsonValue DeviceSettings::moduleSetting(const QString &moduleId, const QString &key) const
{
    const QVariant stored = d->settings->value(moduleSettingKey(moduleId, key));
    if (!stored.isValid())
        return QJsonValue(QJsonValue::Undefined);
    const QByteArray text = '[' + stored.toString().toUtf8() + ']';
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(text, &error);
    if (error.error != QJsonParseError::NoError || !document.isArray() || document.array().size() != 1)
        return stored.toString();
    return document.array().first();
}

void DeviceSettings::setModuleSetting(const QString &moduleId, const QString &key, const QJsonValue &value)
{
    const QString settingKey = moduleSettingKey(moduleId, key);
    if (value.isUndefined() || value.isNull()) {
        d->settings->remove(settingKey);
        return;
    }
    const QByteArray array = QJsonDocument(QJsonArray{value}).toJson(QJsonDocument::Compact);
    d->settings->setValue(settingKey, QString::fromUtf8(array.mid(1, array.size() - 2)));
}

} // namespace mirante
