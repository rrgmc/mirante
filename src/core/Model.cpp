#include "core/Model.h"

#include <QHash>
#include <QStringList>

#include <algorithm>
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

    // Maps each item of task to the task, unless another task holds it first.
    void indexItems(const Task &task)
    {
        for (const Item &item : task.items) {
            if (!taskIdOfItem.contains(item.id))
                taskIdOfItem.insert(item.id, task.id);
        }
    }

    // Removes the items of task from the index. An item id that a task in
    // tasks also holds is mapped to that task instead.
    void unindexItems(const Task &task)
    {
        for (const Item &item : task.items) {
            const auto it = taskIdOfItem.find(item.id);
            if (it == taskIdOfItem.end() || it.value() != task.id)
                continue;
            taskIdOfItem.erase(it);
            for (const auto &[otherId, other] : tasks) {
                const bool holds = std::any_of(other.items.begin(), other.items.end(),
                                               [&item](const Item &i) { return i.id == item.id; });
                if (holds) {
                    taskIdOfItem.insert(item.id, otherId);
                    break;
                }
            }
        }
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

const Dashboard *findDashboard(const Task &task, const QString &dashboardId)
{
    for (const Dashboard &dashboard : task.dashboards) {
        if (dashboard.id == dashboardId)
            return &dashboard;
    }
    return nullptr;
}

// Normalises the layout of a dashboard given to a setter. old is the stored
// dashboard with the same id, or null. A "layout" kept in extra, because the
// file's layout could not be read, is stale once the layout changes. It is
// dropped if it came from old; one that came with the new value is kept.
void normaliseLayout(Dashboard &dashboard, const Dashboard *old, const RefExists &exists)
{
    dashboard.layout = layout::normalise(dashboard.layout, exists);
    const QString key = QStringLiteral("layout");
    if (old && dashboard.extra.contains(key) && old->extra.value(key) == dashboard.extra.value(key)
        && layoutToJson(dashboard.layout) != layoutToJson(old->layout))
        dashboard.extra.remove(key);
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
    for (const auto &entry : d->tasks)
        d->indexItems(entry.second);
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

bool Model::taskInTrash(const Task &task) const
{
    if (task.isTrashed())
        return true;
    const Project *p = project(task.projectId);
    return p != nullptr && p->isTrashed();
}

bool Model::taskInTrash(const QString &taskId) const
{
    const Task *t = task(taskId);
    return t != nullptr && taskInTrash(*t);
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

void Model::setGroup(const Group &group)
{
    if (group.id.isEmpty())
        return;
    const auto it = d->groups.find(group.id);
    if (it == d->groups.end()) {
        d->groups.emplace(group.id, group);
        emit groupAdded(group.id);
        return;
    }
    if (it->second == group)
        return;
    it->second = group;
    emit groupChanged(group.id);
}

void Model::setProject(const Project &project)
{
    if (project.id.isEmpty())
        return;
    const auto it = d->projects.find(project.id);
    if (it == d->projects.end()) {
        d->projects.emplace(project.id, project);
        emit projectAdded(project.id);
        return;
    }
    if (it->second == project)
        return;
    it->second = project;
    emit projectChanged(project.id);
}

void Model::setTask(const Task &task)
{
    if (task.id.isEmpty())
        return;
    const auto it = d->tasks.find(task.id);
    const Task *stored = it == d->tasks.end() ? nullptr : &it->second;

    // Normalises against the new task's items, as setTaskDashboardLayout()
    // does against the stored ones.
    const RefExists exists = [this, &task](const CellRef &ref) {
        if (ref.isItem()) {
            if (!ref.task.isEmpty() && ref.task != task.id)
                return false;
            return std::any_of(task.items.begin(), task.items.end(),
                               [&ref](const Item &item) { return item.id == ref.item; });
        }
        return refExists(ref);
    };
    Task normalised = task;
    for (Dashboard &dashboard : normalised.dashboards)
        normaliseLayout(dashboard, stored ? findDashboard(*stored, dashboard.id) : nullptr, exists);

    const QString id = task.id;
    if (!stored) {
        d->indexItems(d->tasks.emplace(id, std::move(normalised)).first->second);
        emit taskAdded(id);
        return;
    }
    if (*stored == normalised)
        return;
    // Replaced first, so unindexing maps a shared item id to the new value.
    const Task old = std::exchange(it->second, std::move(normalised));
    d->unindexItems(old);
    d->indexItems(it->second);
    emit taskChanged(id);
}

void Model::setRootDashboard(const RootDashboard &dashboard)
{
    if (dashboard.id.isEmpty())
        return;
    const auto it = d->rootDashboards.find(dashboard.id);
    RootDashboard normalised = dashboard;
    normaliseLayout(normalised, it == d->rootDashboards.end() ? nullptr : &it->second,
                    [this](const CellRef &ref) { return refExists(ref); });
    const QString id = dashboard.id;
    if (it == d->rootDashboards.end()) {
        d->rootDashboards.emplace(id, std::move(normalised));
        emit rootDashboardAdded(id);
        return;
    }
    if (it->second == normalised)
        return;
    it->second = std::move(normalised);
    emit rootDashboardChanged(id);
}

// Each remover copies the id first, because the caller may pass the id of the
// entity it removes. Children are collected before any is removed, because a
// slot connected to a removed signal may change the model.

void Model::removeGroup(const QString &groupId)
{
    const QString id = groupId;
    if (d->groups.count(id) == 0)
        return;
    QStringList projectIds;
    for (const Project *project : projectsOfGroup(id))
        projectIds.append(project->id);
    for (const QString &projectId : projectIds)
        removeProject(projectId);
    d->groups.erase(id);
    emit groupRemoved(id);
}

void Model::removeProject(const QString &projectId)
{
    const QString id = projectId;
    if (d->projects.count(id) == 0)
        return;
    QStringList taskIds;
    for (const Task *task : tasksOfProject(id))
        taskIds.append(task->id);
    for (const QString &taskId : taskIds)
        removeTask(taskId);
    d->projects.erase(id);
    emit projectRemoved(id);
}

void Model::removeTask(const QString &taskId)
{
    const QString id = taskId;
    const auto it = d->tasks.find(id);
    if (it == d->tasks.end())
        return;
    // Erased first, so unindexing maps a shared item id to another task only.
    const Task removed = std::move(it->second);
    d->tasks.erase(it);
    d->unindexItems(removed);
    emit taskRemoved(id);
}

QString Model::workspaceName() const { return d->workspaceName; }

void Model::setWorkspaceName(const QString &name)
{
    if (name == d->workspaceName)
        return;
    d->workspaceName = name;
    emit workspaceNameChanged();
}

} // namespace mirante
