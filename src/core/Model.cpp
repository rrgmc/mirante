#include "core/Model.h"

#include <QHash>

#include <map>
#include <utility>

namespace mirante {

// std::map keeps the address of each value stable while other entries are
// added or removed. Items and dashboards live in their task's vectors, which
// only change when the task is replaced.
struct Model::Private
{
    QString workspaceName;
    std::map<QString, Group> groups;
    std::map<QString, Project> projects;
    std::map<QString, Task> tasks;
    std::map<QString, RootDashboard> rootDashboards;
    QHash<QString, QString> taskIdOfItem;    // item id to the id of the task that holds it

    Task *task(const QString &id)
    {
        const auto it = tasks.find(id);
        return it == tasks.end() ? nullptr : &it->second;
    }

    RootDashboard *rootDashboard(const QString &id)
    {
        const auto it = rootDashboards.find(id);
        return it == rootDashboards.end() ? nullptr : &it->second;
    }
};

namespace {

template <typename T>
const T *find(const std::map<QString, T> &map, const QString &id)
{
    const auto it = map.find(id);
    return it == map.end() ? nullptr : &it->second;
}

Dashboard *findDashboard(Task &task, const QString &dashboardId)
{
    for (Dashboard &dashboard : task.dashboards) {
        if (dashboard.id == dashboardId)
            return &dashboard;
    }
    return nullptr;
}

// Stores a new layout. extra wins when the dashboard is written, so a stale
// "layout" kept there would overwrite the edit.
void replaceLayout(Dashboard &dashboard, LayoutNode layout)
{
    dashboard.layout = std::move(layout);
    dashboard.extra.remove(QStringLiteral("layout"));
}

} // namespace

Model::Model(QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
}

Model::~Model() = default;

void Model::reset(std::vector<Group> groups, std::vector<Project> projects, std::vector<Task> tasks,
                  std::vector<RootDashboard> rootDashboards)
{
    d->workspaceName.clear();
    d = std::make_unique<Private>();
    for (Group &group : groups) {
        const QString id = group.id;
        d->groups.insert_or_assign(id, std::move(group));
    }
    for (Project &project : projects) {
        const QString id = project.id;
        d->projects.insert_or_assign(id, std::move(project));
    }
    for (Task &task : tasks) {
        const QString id = task.id;
        d->tasks.insert_or_assign(id, std::move(task));
    }
    for (RootDashboard &dashboard : rootDashboards) {
        const QString id = dashboard.id;
        d->rootDashboards.insert_or_assign(id, std::move(dashboard));
    }
    for (const auto &[taskId, task] : d->tasks) {
        for (const Item &item : task.items) {
            if (!d->taskIdOfItem.contains(item.id))
                d->taskIdOfItem.insert(item.id, taskId);
        }
    }
    emit modelReset();
}

const Group *Model::group(const QString &id) const { return find(d->groups, id); }
const Project *Model::project(const QString &id) const { return find(d->projects, id); }
const Task *Model::task(const QString &id) const { return find(d->tasks, id); }

const Item *Model::item(const QString &taskId, const QString &itemId) const
{
    const Task *t = task(taskId);
    if (!t)
        return nullptr;
    for (const Item &item : t->items) {
        if (item.id == itemId)
            return &item;
    }
    return nullptr;
}

const RootDashboard *Model::rootDashboard(const QString &id) const { return find(d->rootDashboards, id); }

const Dashboard *Model::taskDashboard(const QString &taskId, const QString &dashboardId) const
{
    Task *t = d->task(taskId);
    return t ? findDashboard(*t, dashboardId) : nullptr;
}

const LayoutNode *Model::layout(const DashboardRef &dashboard) const
{
    const Dashboard *found = dashboard.isRoot()
        ? rootDashboard(dashboard.dashboardId)
        : taskDashboard(dashboard.taskId, dashboard.dashboardId);
    return found ? &found->layout : nullptr;
}

const Task *Model::taskOfItem(const QString &itemId) const
{
    const auto it = d->taskIdOfItem.constFind(itemId);
    return it == d->taskIdOfItem.constEnd() ? nullptr : task(it.value());
}

std::vector<const Group *> Model::groups() const
{
    std::vector<const Group *> result;
    result.reserve(d->groups.size());
    for (const auto &entry : d->groups)
        result.push_back(&entry.second);
    return result;
}

std::vector<const Project *> Model::projectsOfGroup(const QString &groupId) const
{
    std::vector<const Project *> result;
    for (const auto &entry : d->projects) {
        if (entry.second.groupId == groupId)
            result.push_back(&entry.second);
    }
    return result;
}

std::vector<const Task *> Model::tasksOfProject(const QString &projectId) const
{
    std::vector<const Task *> result;
    for (const auto &entry : d->tasks) {
        if (entry.second.projectId == projectId)
            result.push_back(&entry.second);
    }
    return result;
}

std::vector<const RootDashboard *> Model::rootDashboards() const
{
    std::vector<const RootDashboard *> result;
    result.reserve(d->rootDashboards.size());
    for (const auto &entry : d->rootDashboards)
        result.push_back(&entry.second);
    return result;
}

bool Model::refExists(const CellRef &ref) const
{
    if (ref.isItem())
        return ref.task.isEmpty() ? taskOfItem(ref.item) != nullptr : item(ref.task, ref.item) != nullptr;
    if (ref.task.isEmpty())
        return false;
    if (!ref.dashboard.isEmpty())
        return taskDashboard(ref.task, ref.dashboard) != nullptr;
    return task(ref.task) != nullptr;
}

void Model::setTaskDashboardLayout(const QString &taskId, const QString &dashboardId, const LayoutNode &tree)
{
    Task *t = d->task(taskId);
    Dashboard *dashboard = t ? findDashboard(*t, dashboardId) : nullptr;
    if (!dashboard)
        return;
    // In a task dashboard an item cell shows an item of that task only.
    const RefExists exists = [this, &taskId](const CellRef &ref) {
        if (ref.isItem())
            return (ref.task.isEmpty() || ref.task == taskId) && item(taskId, ref.item) != nullptr;
        return refExists(ref);
    };
    replaceLayout(*dashboard, layout::normalise(tree, exists));
    emit taskChanged(taskId);
}

void Model::setRootDashboardLayout(const QString &id, const LayoutNode &tree)
{
    RootDashboard *dashboard = d->rootDashboard(id);
    if (!dashboard)
        return;
    replaceLayout(*dashboard, layout::normalise(tree, [this](const CellRef &ref) { return refExists(ref); }));
    emit rootDashboardChanged(id);
}

void Model::setLayout(const DashboardRef &dashboard, const LayoutNode &tree)
{
    if (dashboard.isRoot())
        setRootDashboardLayout(dashboard.dashboardId, tree);
    else
        setTaskDashboardLayout(dashboard.taskId, dashboard.dashboardId, tree);
}

void Model::addRootDashboard(const RootDashboard &dashboard)
{
    // An empty or existing id is ignored, so pointers to the existing one stay valid.
    if (dashboard.id.isEmpty() || d->rootDashboards.count(dashboard.id) != 0)
        return;
    RootDashboard added = dashboard;
    replaceLayout(added, layout::normalise(dashboard.layout, [this](const CellRef &ref) { return refExists(ref); }));
    d->rootDashboards.emplace(added.id, std::move(added));
    emit rootDashboardAdded(dashboard.id);
}

void Model::removeRootDashboard(const QString &id)
{
    if (d->rootDashboards.erase(id) == 0)
        return;
    emit rootDashboardRemoved(id);
}

// Stubs written by T0 of the M3 plan. T1 implements them.
void Model::setGroup(const Group &) {}
void Model::setProject(const Project &) {}
void Model::setTask(const Task &) {}
void Model::setRootDashboard(const RootDashboard &) {}
void Model::removeGroup(const QString &) {}
void Model::removeProject(const QString &) {}
void Model::removeTask(const QString &) {}

QString Model::workspaceName() const { return d->workspaceName; }

void Model::setWorkspaceName(const QString &name)
{
    if (name == d->workspaceName)
        return;
    d->workspaceName = name;
    emit workspaceNameChanged();
}

} // namespace mirante
