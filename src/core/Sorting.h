#pragma once

#include "core/Entities.h"

#include <set>
#include <vector>

// Display order of projects and tasks: the user's manual order. Priority is
// shown, not sorted by, except in Focus. See docs/data-model.md#priority and
// docs/decisions/0034-manual-order-in-the-sidebar.md. Implemented in Sorting.cpp.

namespace mirante {

class Model;

struct SortFilter
{
    std::set<Priority> hiddenPriorities;    // projects and tasks of these levels are hidden
    bool closedTasks = false;   // false: only active tasks; true: only done and archived ones
    bool hideEmptyProjects = false;  // hide projects with no task in the list; used by the sidebar

    bool operator==(const SortFilter &other) const
    {
        return hiddenPriorities == other.hiddenPriorities && closedTasks == other.closedTasks
            && hideEmptyProjects == other.hideEmptyProjects;
    }
    bool operator!=(const SortFilter &other) const { return !(*this == other); }
};

bool projectBefore(const Project &a, const Project &b);
bool taskBefore(const Task &a, const Task &b);

bool projectVisible(const Project &project, const SortFilter &filter);
bool taskVisible(const Task &task, const SortFilter &filter);

// Filters and sorts. The input order does not matter.
std::vector<const Project *> sortedProjects(std::vector<const Project *> projects, const SortFilter &filter);
std::vector<const Task *> sortedTasks(std::vector<const Task *> tasks, const SortFilter &filter);

// The Focus order of docs/ui.md#focus-view: active tasks only, by task
// priority, then project priority, then the task's order. model gives each
// task's project. The input order does not matter.
std::vector<const Task *> focusOrder(std::vector<const Task *> tasks, const Model &model);

} // namespace mirante
