#include "platform/Tray.h"

// macOS has its own implementation in TrayMac.mm.
#if !defined(Q_OS_MACOS)

#include <QIcon>
#include <QMenu>
#include <QSystemTrayIcon>

namespace mirante::platform {

struct Tray::Private
{
    QSystemTrayIcon *icon = nullptr;
};

bool Tray::isAvailable() { return QSystemTrayIcon::isSystemTrayAvailable(); }

Tray::Tray(const QIcon &icon, const QString &toolTip, QMenu *menu, QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
    d->icon = new QSystemTrayIcon(icon, this);
    d->icon->setToolTip(toolTip);
    d->icon->setContextMenu(menu);
    connect(d->icon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger)
            emit triggered();
    });
    d->icon->show();
}

Tray::~Tray() = default;

void Tray::setDockIconVisible(bool visible) { Q_UNUSED(visible); }

} // namespace mirante::platform

#endif
