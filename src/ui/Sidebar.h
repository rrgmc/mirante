#pragma once

#include "ui/SidebarModel.h"

#include <QWidget>

#include <functional>
#include <memory>

class QMenu;

namespace mirante {

class ItemTypeRegistry;
class Model;

// The sidebar: a tree view on SidebarModel, with a delegate for priority
// markers and dimmed Low entries, two filter buttons and a collapse button.
// See the M2 plan, part 5. Implemented in Sidebar.cpp.
class Sidebar : public QWidget
{
    Q_OBJECT

public:
    Sidebar(const Model &model, const ItemTypeRegistry &types, QWidget *parent = nullptr);
    ~Sidebar() override;

    // Selects and scrolls to a group, project, task or item, switching to the
    // task list that holds it.
    void reveal(const QString &id);

    // The title above the tree: the workspace's name.
    void setTitle(const QString &title);

    // Expand or collapse every task, with its Dashboards and Items lists.
    // Groups and projects are left as they are.
    void expandAllTasks();
    void collapseAllTasks();

    // Tints the row of a task or item, the one the hovered dashboard cell
    // shows. If a collapsed task, project or group hides that row, its row is
    // tinted instead, and the tint follows as rows expand or collapse. Empty
    // removes the tint. The tree does not expand or scroll.
    void setHoverHighlight(const QString &id);
    QString hoverHighlight() const;     // the id given to setHoverHighlight()
    QString hoverRow() const;           // the id of the row tinted for it, or empty

    SidebarModel *model() const;
    // The selected row, or an invalid index.
    QModelIndex currentIndex() const;

    // Adds a widget to the header, before the expand and collapse buttons,
    // such as the main window's + menu.
    void addHeaderWidget(QWidget *widget);

    // Fills the context menu of a row with the entity commands, such as
    // EntityActions::fillMenu. The sidebar adds its own commands, such as "Open
    // in new root dashboard", after them. Without a filler, only its own appear.
    using MenuFiller = std::function<void(QMenu *menu, SidebarModel::Kind kind, const QString &id,
                                          const QString &taskId)>;
    void setMenuFiller(MenuFiller filler);
    // The same for rows of the Trash list, such as EntityActions::fillTrashMenu.
    // The sidebar adds nothing of its own there.
    void setTrashMenuFiller(MenuFiller filler);

    // Whether the Trash list is the current one.
    bool showsTrash() const;

signals:
    // The Focus entry above the tree was activated.
    void focusRequested();
    // A double click, or Enter, opens an item or a task dashboard. On every
    // other row, tasks included, it expands or collapses the row. A single
    // click only selects.
    void itemOpened(const QString &taskId, const QString &itemId);
    void dashboardOpened(const QString &taskId, const QString &dashboardId);
    // From the context menu of a task dashboard or an item.
    void openInNewRootDashboard(const QString &taskId, const QString &dashboardId);
    void openItemInNewRootDashboard(const QString &taskId, const QString &itemId);
    void collapseRequested();
    // A double click on the title, to rename the workspace.
    void titleDoubleClicked();
    // The "Empty Trash…" button, shown under the Trash list, was clicked.
    void emptyTrashRequested();

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
