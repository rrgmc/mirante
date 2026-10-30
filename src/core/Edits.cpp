#include "core/Edits.h"

#include "core/Sorting.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QUuid>

#include <algorithm>
#include <cstddef>
#include <utility>

namespace mirante::edits {

namespace {

// Replaces the element with the same id, or appends the value.
template <typename T>
void putById(std::vector<T> &list, const T &value)
{
    const auto it = std::find_if(list.begin(), list.end(), [&value](const T &e) { return e.id == value.id; });
    if (it == list.end())
        list.push_back(value);
    else
        *it = value;
}

template <typename T>
bool containsId(const std::vector<T> &list, const QString &id)
{
    return std::any_of(list.begin(), list.end(), [&id](const T &e) { return e.id == id; });
}

// Moves the element with id to index among the other elements, clamped. An
// unknown id changes nothing.
template <typename T>
void moveInList(std::vector<T> &list, const QString &id, int index)
{
    const auto it = std::find_if(list.begin(), list.end(), [&id](const T &e) { return e.id == id; });
    if (it == list.end())
        return;
    T moved = std::move(*it);
    list.erase(it);
    index = std::clamp(index, 0, static_cast<int>(list.size()));
    list.insert(list.begin() + index, std::move(moved));
}

// Moves the entry with id to index among the entries for which inList is true,
// in the order given by before, and renumbers order in that list from 0.
// setList is called on the moved entry first, to put it in the list. Returns
// every entry whose JSON changed.
template <typename T, typename InList, typename SetList, typename Before>
std::vector<T> move(std::vector<T> entries, const QString &id, int index, InList inList, SetList setList,
                    Before before)
{
    const auto moved = std::find_if(entries.begin(), entries.end(), [&id](const T &e) { return e.id == id; });
    if (moved == entries.end())
        return {};
    const std::vector<T> original = entries;
    setList(*moved);

    std::vector<T *> list;
    for (T &entry : entries) {
        if (&entry != &*moved && inList(entry))
            list.push_back(&entry);
    }
    std::sort(list.begin(), list.end(), [&before](const T *a, const T *b) { return before(*a, *b); });
    index = std::clamp(index, 0, static_cast<int>(list.size()));
    list.insert(list.begin() + index, &*moved);
    for (int i = 0; i < static_cast<int>(list.size()); ++i)
        list[static_cast<std::size_t>(i)]->order = i;

    std::vector<T> changed;
    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (entries[i] != original[i])
            changed.push_back(entries[i]);
    }
    return changed;
}

// The sidebar's order of groups: order, then name, then id.
bool groupBefore(const Group &a, const Group &b)
{
    if (a.order != b.order)
        return a.order < b.order;
    if (const int c = QString::compare(a.name, b.name, Qt::CaseInsensitive); c != 0)
        return c < 0;
    return a.id < b.id;
}

void collectStrings(const QJsonValue &value, QSet<QString> &out)
{
    if (value.isString()) {
        out.insert(value.toString());
    } else if (value.isArray()) {
        for (const QJsonValue &element : value.toArray())
            collectStrings(element, out);
    } else if (value.isObject()) {
        const QJsonObject object = value.toObject();
        for (auto it = object.begin(); it != object.end(); ++it)
            collectStrings(it.value(), out);
    }
}

} // namespace

QString newId() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }

Task newTask(const QString &projectId, const QString &title)
{
    Dashboard dashboard;
    dashboard.id = newId();
    dashboard.name = QStringLiteral("Main");
    dashboard.layout = layout::emptyLayout();

    Task task;
    task.id = newId();
    task.projectId = projectId;
    task.title = title;
    task.defaultDashboardId = dashboard.id;
    task.dashboards.push_back(std::move(dashboard));
    return task;
}

// An item with an id the task already holds replaces that item.
Task addItem(Task task, const Item &item)
{
    putById(task.items, item);
    return task;
}

Task removeItem(Task task, const QString &itemId)
{
    if (!containsId(task.items, itemId))
        return task;
    task.items.erase(std::remove_if(task.items.begin(), task.items.end(),
                                    [&itemId](const Item &item) { return item.id == itemId; }),
                     task.items.end());
    for (Dashboard &dashboard : task.dashboards) {
        for (const NodePath &path : layout::cellPaths(dashboard.layout)) {
            const LayoutNode *cell = layout::nodeAt(dashboard.layout, path);
            if (cell->ref && cell->ref->item == itemId)
                dashboard.layout = layout::setRef(dashboard.layout, path, std::nullopt);
        }
    }
    return task;
}

// A dashboard with an id the task already holds replaces that dashboard. The
// first dashboard of a task becomes its default.
Task addDashboard(Task task, const Dashboard &dashboard)
{
    putById(task.dashboards, dashboard);
    if (!containsId(task.dashboards, task.defaultDashboardId))
        task.defaultDashboardId = task.dashboards.front().id;
    return task;
}

Task removeDashboard(Task task, const QString &dashboardId)
{
    if (task.dashboards.size() < 2 || !containsId(task.dashboards, dashboardId))
        return task;
    task.dashboards.erase(std::remove_if(task.dashboards.begin(), task.dashboards.end(),
                                         [&dashboardId](const Dashboard &d) { return d.id == dashboardId; }),
                          task.dashboards.end());
    if (!containsId(task.dashboards, task.defaultDashboardId))
        task.defaultDashboardId = task.dashboards.front().id;
    return task;
}

// An unknown dashboard id leaves the task unchanged.
Task setDefaultDashboard(Task task, const QString &dashboardId)
{
    if (containsId(task.dashboards, dashboardId))
        task.defaultDashboardId = dashboardId;
    return task;
}

Task moveItem(Task task, const QString &itemId, int index)
{
    moveInList(task.items, itemId, index);
    return task;
}

Task moveDashboard(Task task, const QString &dashboardId, int index)
{
    moveInList(task.dashboards, dashboardId, index);
    return task;
}

std::vector<Project> moveProject(std::vector<Project> siblings, const QString &id, int index)
{
    return move(std::move(siblings), id, index,
                [](const Project &) { return true; },
                [](Project &) {},
                projectBefore);
}

std::vector<Task> moveTask(std::vector<Task> siblings, const QString &id, int index)
{
    return move(std::move(siblings), id, index,
                [](const Task &) { return true; },
                [](Task &) {},
                taskBefore);
}

std::vector<Group> moveGroup(std::vector<Group> groups, const QString &id, int index)
{
    return move(std::move(groups), id, index,
                [](const Group &) { return true; },
                [](Group &) {},
                groupBefore);
}

QSet<QString> referencedStrings(const std::vector<const Task *> &tasks)
{
    QSet<QString> strings;
    for (const Task *task : tasks) {
        if (!task)
            continue;
        for (const Item &item : task->items)
            collectStrings(item.config, strings);
    }
    return strings;
}

} // namespace mirante::edits
