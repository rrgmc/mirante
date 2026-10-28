#pragma once

#include "core/LayoutTree.h"
#include "core/Model.h"
#include "layout/DragMime.h"
#include "layout/DropOverlay.h"

#include <QObject>

#include <memory>

namespace mirante {

class ItemTypeRegistry;
class LayoutView;

// Turns menu commands and drops into layout tree edits, and stores the result
// through Model, which normalises. See the M2 plan, part 4.
// Implemented in LayoutEditor.cpp.
class LayoutEditor : public QObject
{
    Q_OBJECT

public:
    LayoutEditor(Model &model, const ItemTypeRegistry &types, QObject *parent = nullptr);
    ~LayoutEditor() override;

    // Connects a view that shows the dashboard: its menus, drags, drops and
    // treeEdited signal. A view is detached when it is destroyed.
    void attach(LayoutView *view, const DashboardRef &dashboard);
    void detach(LayoutView *view);

    // The commands of the cell and tab menus. Public so tests can call them.
    enum class CellCommand { SplitRight, SplitDown, WrapInTabs, AddTab, ClearCell, RemoveCell };
    // AddTab inserts an empty tab after the given one, and makes it current.
    enum class TabCommand { AddTab, Close, MoveLeft, MoveRight, ToggleCollapseSingle };

    void cellCommand(const DashboardRef &dashboard, const NodePath &path, CellCommand command);
    void changeItem(const DashboardRef &dashboard, const NodePath &path, const CellRef &ref);
    void tabCommand(const DashboardRef &dashboard, const NodePath &tabsPath, int index, TabCommand command);
    void renameTab(const DashboardRef &dashboard, const NodePath &tabsPath, int index, const QString &title);

    // Applies a drop, following the rules in docs/ui.md#editing-layouts.
    // Returns false if the drop is not allowed, such as an item of another task
    // dropped on a task dashboard.
    bool drop(const DashboardRef &dashboard, const DropTarget &target, const DragPayload &payload);

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
