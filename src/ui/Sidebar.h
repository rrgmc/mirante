#pragma once

#include <QWidget>

#include <memory>

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

    // Expand or collapse every task, with its Dashboards and Items lists.
    // Groups and projects are left as they are.
    void expandAllTasks();
    void collapseAllTasks();

signals:
    // A double click, or Enter, opens an item or a task dashboard. On every
    // other row, tasks included, it expands or collapses the row. A single
    // click only selects.
    void itemOpened(const QString &taskId, const QString &itemId);
    void dashboardOpened(const QString &taskId, const QString &dashboardId);
    // From the context menu of a task dashboard or an item.
    void openInNewRootDashboard(const QString &taskId, const QString &dashboardId);
    void openItemInNewRootDashboard(const QString &taskId, const QString &itemId);
    void collapseRequested();

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
