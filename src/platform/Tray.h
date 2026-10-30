#pragma once

#include <QObject>
#include <QString>

#include <memory>

class QIcon;
class QMenu;

namespace mirante::platform {

// The icon in the Windows notification area, the macOS menu bar or the Linux
// system tray. See docs/ui.md#tray-and-notifications.
//
// On macOS it is a native NSStatusItem, in TrayMac.mm. Qt 6.8's QSystemTrayIcon
// aborts on macOS 27 when the icon is clicked: it reads the click count of an
// event that is no longer a mouse event. Qt 6.12 fixes this. Elsewhere it wraps
// QSystemTrayIcon, in Tray.cpp.
class Tray : public QObject
{
    Q_OBJECT

public:
    // False where there is no tray, for example GNOME without an extension, or
    // at sign-in before the taskbar has started.
    static bool isAvailable();

    // The menu must outlive the tray. The macOS menu bar shows monochromeIcon
    // as a template image: macOS draws its alpha in the menu bar's colour, so
    // it matches light and dark menu bars. Elsewhere the tray shows icon.
    Tray(const QIcon &icon, const QIcon &monochromeIcon, const QString &toolTip, QMenu *menu,
         QObject *parent = nullptr);
    ~Tray() override;

    // Shows or removes the Dock icon while the window is shown or hidden in the
    // menu bar. Showing it also brings Mirante to the front. Does nothing
    // outside macOS.
    void setDockIconVisible(bool visible);

signals:
    // A left click. Not emitted on macOS, where any click opens the menu.
    void triggered();
    // macOS only: the app was opened again from the Finder or the Dock. A second
    // process does not start there, so this is its way to bring the window back.
    void reopened();

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante::platform
