#pragma once

#include "core/Entities.h"
#include "core/LayoutTree.h"

#include <QObject>

#include <memory>
#include <vector>

namespace mirante {

// Names one dashboard: a task dashboard (taskId and dashboardId), or a root
// dashboard (dashboardId only).
struct DashboardRef
{
    QString taskId;
    QString dashboardId;

    bool isRoot() const { return taskId.isEmpty(); }

    friend bool operator==(const DashboardRef &a, const DashboardRef &b)
    {
        return a.taskId == b.taskId && a.dashboardId == b.dashboardId;
    }
    friend bool operator!=(const DashboardRef &a, const DashboardRef &b) { return !(a == b); }
};

// Owns all entities. All changes go through its methods, and each change emits
// a signal. See docs/architecture.md#core. Implemented in Model.cpp.
//
// Pointers returned by lookups stay valid until the next change that removes
// or replaces that entity, or the next reset().
class Model : public QObject
{
    Q_OBJECT

public:
    explicit Model(QObject *parent = nullptr);
    ~Model() override;

    // Replaces everything. Used by the Store after loading. Emits modelReset().
    void reset(std::vector<Group> groups, std::vector<Project> projects, std::vector<Task> tasks,
               std::vector<RootDashboard> rootDashboards);

    // Lookups. Each returns nullptr if the id is unknown.
    const Group *group(const QString &id) const;
    const Project *project(const QString &id) const;
    const Task *task(const QString &id) const;
    const Item *item(const QString &taskId, const QString &itemId) const;
    const RootDashboard *rootDashboard(const QString &id) const;
    const Dashboard *taskDashboard(const QString &taskId, const QString &dashboardId) const;
    const LayoutNode *layout(const DashboardRef &dashboard) const;
    const Task *taskOfItem(const QString &itemId) const;

    // Children, unsorted. Use Sorting for display order.
    std::vector<const Group *> groups() const;
    std::vector<const Project *> projectsOfGroup(const QString &groupId) const;
    std::vector<const Task *> tasksOfProject(const QString &projectId) const;
    std::vector<const RootDashboard *> rootDashboards() const;

    // Whether a cell reference still refers to something. Used for normalisation.
    bool refExists(const CellRef &ref) const;

    // Changes in M2. Each normalises the tree before storing it.
    void setTaskDashboardLayout(const QString &taskId, const QString &dashboardId, const LayoutNode &tree);
    void setRootDashboardLayout(const QString &id, const LayoutNode &tree);
    void setLayout(const DashboardRef &dashboard, const LayoutNode &tree);   // calls one of the two above
    void addRootDashboard(const RootDashboard &dashboard);

signals:
    void modelReset();
    void taskChanged(const QString &taskId);
    void rootDashboardChanged(const QString &id);
    void rootDashboardAdded(const QString &id);

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
