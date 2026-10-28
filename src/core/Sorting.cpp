#include "core/Sorting.h"

#include <algorithm>
#include <utility>

namespace mirante {

namespace {

int compareText(const QString &a, const QString &b)
{
    return QString::compare(a, b, Qt::CaseInsensitive);
}

// Removes null pointers and hidden entries, then sorts what is left.
template<typename T, typename Visible, typename Before>
std::vector<const T *> filterAndSort(std::vector<const T *> list, Visible visible, Before before)
{
    list.erase(std::remove_if(list.begin(), list.end(),
                              [&](const T *entry) { return entry == nullptr || !visible(*entry); }),
               list.end());
    std::sort(list.begin(), list.end(), [&](const T *a, const T *b) { return before(*a, *b); });
    return list;
}

} // namespace

// Priority, then order, then name, then id. The last two keys make the order
// stable when the user has not ordered the entries.
bool projectBefore(const Project &a, const Project &b)
{
    if (a.priority != b.priority)
        return a.priority < b.priority;
    if (a.order != b.order)
        return a.order < b.order;
    if (const int c = compareText(a.name, b.name); c != 0)
        return c < 0;
    return a.id < b.id;
}

// The same keys as projects, with the title.
bool taskBefore(const Task &a, const Task &b)
{
    if (a.priority != b.priority)
        return a.priority < b.priority;
    if (a.order != b.order)
        return a.order < b.order;
    if (const int c = compareText(a.title, b.title); c != 0)
        return c < 0;
    return a.id < b.id;
}

// closedTasks does not apply to projects, which have no state.
bool projectVisible(const Project &project, const SortFilter &filter)
{
    return !(filter.hideLow && project.priority == Priority::Low);
}

bool taskVisible(const Task &task, const SortFilter &filter)
{
    if (filter.hideLow && task.priority == Priority::Low)
        return false;
    return (task.state != TaskState::Active) == filter.closedTasks;
}

std::vector<const Project *> sortedProjects(std::vector<const Project *> projects, const SortFilter &filter)
{
    return filterAndSort(std::move(projects),
                         [&](const Project &p) { return projectVisible(p, filter); },
                         projectBefore);
}

std::vector<const Task *> sortedTasks(std::vector<const Task *> tasks, const SortFilter &filter)
{
    return filterAndSort(std::move(tasks),
                         [&](const Task &t) { return taskVisible(t, filter); },
                         taskBefore);
}

} // namespace mirante
