#include "core/DeviceSettings.h"

#include <QSettings>

// Stubs written by T0 of the M3 plan. T4 implements them.

namespace mirante {

struct DeviceSettings::Private
{
};

DeviceSettings::DeviceSettings(QSettings *)
    : d(std::make_unique<Private>())
{
}

DeviceSettings::~DeviceSettings() = default;

QString DeviceSettings::lastWorkspace() const { return {}; }
void DeviceSettings::setLastWorkspace(const QString &) {}
QStringList DeviceSettings::recentWorkspaces() const { return {}; }
void DeviceSettings::addRecentWorkspace(const QString &) {}
void DeviceSettings::removeRecentWorkspace(const QString &) {}
void DeviceSettings::clearRecentWorkspaces() {}
QString DeviceSettings::folderPath(const QString &) const { return {}; }
void DeviceSettings::setFolderPath(const QString &, const QString &) {}
DeviceSettings::Tabs DeviceSettings::openTabs(const QString &) const { return {}; }
void DeviceSettings::setOpenTabs(const QString &, const Tabs &) {}
QSize DeviceSettings::windowSize() const { return {}; }
void DeviceSettings::setWindowSize(const QSize &) {}
bool DeviceSettings::windowMaximized() const { return false; }
void DeviceSettings::setWindowMaximized(bool) {}
QByteArray DeviceSettings::sidebarSplitterState() const { return {}; }
void DeviceSettings::setSidebarSplitterState(const QByteArray &) {}

} // namespace mirante
