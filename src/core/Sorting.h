#pragma once

#include "core/Entities.h"

#include <vector>

// Display order of projects and tasks: priority, then order.
// See docs/data-model.md#priority. Implemented in Sorting.cpp.

namespace mirante {

struct SortFilter
{
    bool hideLow = false;       // hide Low projects and tasks
    bool closedTasks = false;   // false: only active tasks; true: only done and archived ones
};

bool projectBefore(const Project &a, const Project &b);
bool taskBefore(const Task &a, const Task &b);

bool projectVisible(const Project &project, const SortFilter &filter);
bool taskVisible(const Task &task, const SortFilter &filter);

// Filters and sorts. The input order does not matter.
std::vector<const Project *> sortedProjects(std::vector<const Project *> projects, const SortFilter &filter);
std::vector<const Task *> sortedTasks(std::vector<const Task *> tasks, const SortFilter &filter);

} // namespace mirante
