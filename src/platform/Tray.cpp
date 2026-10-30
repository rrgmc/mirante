#include "platform/Tray.h"

#include "platform/TrayBadge.h"

// macOS has its own implementation in TrayMac.mm.
#if !defined(Q_OS_MACOS)

#include <QIcon>
#include <QMenu>
#include <QSystemTrayIcon>

namespace mirante::platform {

struct Tray::Private
{
    QSystemTrayIcon *icon = nullptr;
    QIcon base;
    int count = 0;
};

bool Tray::isAvailable() { return QSystemTrayIcon::isSystemTrayAvailable(); }

Tray::Tray(const QIcon &icon, const QString &toolTip, QMenu *menu, QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
    d->base = icon;
    d->icon = new QSystemTrayIcon(icon, this);
    d->icon->setToolTip(toolTip);
    d->icon->setContextMenu(menu);
    connect(d->icon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger)
            emit triggered();
    });
    connect(d->icon, &QSystemTrayIcon::messageClicked, this, &Tray::messageClicked);
    d->icon->show();
}

Tray::~Tray() = default;

void Tray::setCount(int count)
{
    if (count == d->count)
        return;
    d->count = count;
    d->icon->setIcon(withTrayBadge(d->base, count));
}

void Tray::setToolTip(const QString &toolTip) { d->icon->setToolTip(toolTip); }

void Tray::showMessage(const QString &title, const QString &text)
{
    d->icon->showMessage(title, text, QSystemTrayIcon::Warning);
}

void Tray::setDockIconVisible(bool visible) { Q_UNUSED(visible); }

} // namespace mirante::platform

#endif
