#pragma once

#include <QIcon>
#include <QImage>
#include <QString>

namespace mirante::platform {

// The attention count on the tray icon. See docs/ui.md#tray-and-notifications.
// Implemented in TrayBadge.cpp.

// The count as a badge shows it: "99+" above 99.
QString trayBadgeText(int count);

// The image with the count in a red badge at its lower right. A count of 0 or
// less gives the image unchanged.
QImage paintTrayBadge(const QImage &image, int count);

// The icon with the badge, at each size a tray may ask for. A count of 0 or
// less gives the icon unchanged.
QIcon withTrayBadge(const QIcon &icon, int count);

} // namespace mirante::platform
