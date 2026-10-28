// Stub from task T0. Task T6 implements this file.
#include "core/Sorting.h"

namespace mirante {

bool projectBefore(const Project & /*a*/, const Project & /*b*/) { return false; }
bool taskBefore(const Task & /*a*/, const Task & /*b*/) { return false; }

bool projectVisible(const Project & /*project*/, const SortFilter & /*filter*/) { return true; }
bool taskVisible(const Task & /*task*/, const SortFilter & /*filter*/) { return true; }

std::vector<const Project *> sortedProjects(std::vector<const Project *> projects, const SortFilter & /*filter*/)
{
    return projects;
}

std::vector<const Task *> sortedTasks(std::vector<const Task *> tasks, const SortFilter & /*filter*/)
{
    return tasks;
}

} // namespace mirante
