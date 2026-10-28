#pragma once

#include "layout/DropOverlay.h"

#include <QRect>

#include <optional>

class QPoint;

namespace mirante {

class LayoutView;

// What DropOverlay shows for a position: the target, and the rectangle to
// highlight in view coordinates. For a tab bar the rectangle is a thin insert
// marker. Private to DropOverlay; declared here so tests can check the zones.
struct DropHit
{
    DropTarget target;
    QRect highlight;
};

// Finds the cell or tab bar of the view under pos, which is in view
// coordinates. Each edge of a cell is its outer 25%; the rest is the centre.
// With replace, the whole cell under pos is a Replace zone, and tab bars do
// not count. Implemented in DropOverlay.cpp.
std::optional<DropHit> dropHitAt(const LayoutView *view, const QPoint &pos, bool replace = false);

} // namespace mirante
