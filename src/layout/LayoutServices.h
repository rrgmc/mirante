#pragma once

#include <QString>

#include <functional>

namespace mirante {

class CellHoverTracker;
class ItemTypeRegistry;
class LayoutEditor;
class LockState;
class Model;
class SingleViewTracker;

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
};

} // namespace mirante
