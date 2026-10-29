#pragma once

#include <QString>

#include <functional>

class QWidget;

namespace mirante {

class CellHoverTracker;
class ItemTypeRegistry;
class LayoutEditor;
class LockState;
class Model;
class SingleViewTracker;
struct PlacementServices;

// The shared objects that cells and nested views need. MainWindow owns them
// and passes this struct down. A null pointer means the feature is off, which
// tests use.
struct LayoutServices
{
    Model *model = nullptr;
    const ItemTypeRegistry *types = nullptr;
    LayoutEditor *editor = nullptr;
    LockState *locks = nullptr;
    SingleViewTracker *tracker = nullptr;
    // Told which cells show which item or task, so the sidebar can tint the
    // row of the hovered one.
    CellHoverTracker *hover = nullptr;

    // Opens a task dashboard on its own, where it can be edited. Task cells in
    // root dashboards call it on a double click or from their menu.
    std::function<void(const QString &taskId, const QString &dashboardId)> openTaskAlone;

    // The bodies of item cells. Null keeps the M2 placeholder, which tests use.
    const PlacementServices *placements = nullptr;

    // A bigger view of an item: maximising a cell over the content area, and
    // opening an item alone in the preview tab. See the M4 plan, part 6.
    std::function<void(QWidget *cell)> toggleMaximised;
    std::function<bool(const QWidget *cell)> isMaximised;
    std::function<void(const QString &taskId, const QString &itemId)> openItemAlone;
};

} // namespace mirante
