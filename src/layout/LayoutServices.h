#pragma once

#include <QString>

#include <functional>

namespace mirante {

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

    // Opens a task dashboard on its own, where it can be edited. Task cells in
    // root dashboards call it on a double click or from their menu.
    std::function<void(const QString &taskId, const QString &dashboardId)> openTaskAlone;
};

} // namespace mirante
