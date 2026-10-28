#pragma once

#include "core/LayoutTree.h"
#include "core/Model.h"

#include <optional>

class QMimeData;

namespace mirante {

inline constexpr char dragMimeType[] = "application/x-mirante-ref";

// What a drag carries: the reference, and for a cell moved within or between
// dashboards, where it came from. A drag from the sidebar has no source.
struct DragPayload
{
    CellRef ref;
    std::optional<DashboardRef> sourceDashboard;
    std::optional<NodePath> sourcePath;
};

// Implemented in DragMime.cpp. The caller owns the returned object.
QMimeData *dragMimeData(const DragPayload &payload);

// Returns nullopt if the data does not hold a valid payload.
std::optional<DragPayload> dragPayload(const QMimeData *mime);

} // namespace mirante
