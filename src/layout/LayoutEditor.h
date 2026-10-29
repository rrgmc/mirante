#pragma once

#include "core/LayoutTree.h"
#include "core/Model.h"
#include "layout/DragMime.h"
#include "layout/DropOverlay.h"

#include <QObject>

#include <functional>
#include <memory>

namespace mirante {

class ItemTypeRegistry;
class LayoutView;
struct LayoutServices;

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
    // Delete removes the tab and its content from the layout. Ungroup unwraps
    // a tab group with one tab: the tab's content takes the group's place.
    enum class TabCommand { AddTab, Delete, MoveLeft, MoveRight, Ungroup };

    void cellCommand(const DashboardRef &dashboard, const NodePath &path, CellCommand command);
    void changeItem(const DashboardRef &dashboard, const NodePath &path, const CellRef &ref);
    void tabCommand(const DashboardRef &dashboard, const NodePath &tabsPath, int index, TabCommand command);
    void renameTab(const DashboardRef &dashboard, const NodePath &tabsPath, int index, const QString &title);

    // Asks the user to confirm a command that removes content. The menus ask
    // before Clear cell, Remove cell and closing a tab; the public commands
    // above do not ask. The default shows a message box; tests replace it.
    void setConfirm(std::function<bool(QWidget *parent, const QString &question)> confirm);

    // The services whose callbacks the item cell menu calls: "Maximise" or
    // "Restore", and "Open Alone". An entry is hidden while its callback is
    // null, and both are hidden without services. The services must outlive
    // the editor. See the M4 plan, part 6.
    void setServices(const LayoutServices *services);

    // Applies a drop, following the rules in docs/ui.md#editing-layouts.
    // Returns false if the drop is not allowed, such as an item of another task
    // dropped on a task dashboard.
    bool drop(const DashboardRef &dashboard, const DropTarget &target, const DragPayload &payload);

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
