#pragma once

#include <QList>
#include <QRect>

namespace mirante::windowplacement {

// Where the main window opens, from the geometry saved when it last closed.
// Pure functions on rectangles, so tests can use fake screens. See
// docs/ui.md#window. Implemented in WindowPlacement.cpp.

// The height assumed for the title bar above a window's client area. The
// saved geometry is the client area, which does not include it.
constexpr int titleBarHeight = 32;

// Whether a window with this client geometry can still be seen and moved on
// these screens, given by their available geometries. Two things must hold:
// the title bar strip above the client area lies wholly on the screens, so the
// window can be dragged, and at least half of the window is on them.
bool isVisible(const QRect &client, const QList<QRect> &screens);

// The client geometry to open the window at. The saved one if it is valid and
// visible; otherwise the saved size, or fallbackSize if none, made to fit the
// primary screen's available geometry and centred on it.
QRect place(const QRect &saved, const QSize &fallbackSize, const QList<QRect> &screens, const QRect &primary);

} // namespace mirante::windowplacement
