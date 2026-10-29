#include "core/DeviceSettings.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QSettings>

namespace mirante {

namespace {

// The keys M2 already used, kept so existing settings carry over.
const QString lastWorkspaceKey = QStringLiteral("workspace/last");
const QString recentWorkspacesKey = QStringLiteral("workspace/recent");
const QString windowSizeKey = QStringLiteral("window/size");
const QString windowMaximizedKey = QStringLiteral("window/maximized");
const QString windowSplitterKey = QStringLiteral("window/splitter");
const QString hiddenViewLimitKey = QStringLiteral("webViews/hiddenLimit");

constexpr int defaultHiddenViewLimit = 8;

constexpr int maxRecent = 10;

QString folderKey(const QString &folderId)
{
    return QStringLiteral("folders/") + folderId;
}

QString detachedKey(const QString &dashboardId)
{
    return QStringLiteral("detached/") + dashboardId;
}

// The group holding one workspace's open tabs. A path can hold characters a
// settings key cannot, so the group is named by a hash of the cleaned absolute
// path.
QString tabsGroup(const QString &workspace)
{
    const QString path = QDir::cleanPath(QFileInfo(workspace).absoluteFilePath());
    const QByteArray hash = QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Sha1).toHex();
    return QStringLiteral("tabs/") + QString::fromLatin1(hash);
}

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

} // namespace mirante
