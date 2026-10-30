#pragma once

#include "core/Entities.h"

#include <QSet>
#include <QString>

#include <vector>

// Changes to entities as pure functions: each takes a value and returns the
// changed value, for the Model's setters. See the M3 plan, part 1.
// Implemented in Edits.cpp.

namespace mirante::edits {

// A new UUID, without braces.
QString newId();

// A new active task with one dashboard, "Main", holding one empty cell, which
// is its default dashboard.
Task newTask(const QString &projectId, const QString &title);

Task addItem(Task task, const Item &item);
// Removes the item, and turns every cell of the task's dashboards that shows
// it into an empty cell.
Task removeItem(Task task, const QString &itemId);
Task addDashboard(Task task, const Dashboard &dashboard);
// Removes the dashboard. A task keeps at least one dashboard, so removing the
// last one returns the task unchanged. If the default goes, the first
// remaining dashboard becomes the default.
Task removeDashboard(Task task, const QString &dashboardId);
Task setDefaultDashboard(Task task, const QString &dashboardId);
// Moves an item or a task dashboard to index within the task's list, which
// keeps them in sidebar order. The index counts the other entries of the
// list and is clamped. An unknown id leaves the task unchanged.
Task moveItem(Task task, const QString &itemId, int index);
Task moveDashboard(Task task, const QString &dashboardId, int index);

// Moves a project or task to index within the list of its siblings, and
// renumbers order in that list from 0. Priority is not changed. siblings are
// all projects of one group, or all tasks of one project, in any order; the
// index counts them all, hidden ones included. Returns every sibling whose
// order changed, ready for the setters.
std::vector<Project> moveProject(std::vector<Project> siblings, const QString &id, int index);
std::vector<Task> moveTask(std::vector<Task> siblings, const QString &id, int index);
// The same for groups, which have no priority.
std::vector<Group> moveGroup(std::vector<Group> groups, const QString &id, int index);

// Every string anywhere in the config values of the tasks' items, at any
// depth. Used to tell whether a file in files/ is still referred to, for any
// module, present or not. See docs/decisions/0030-deleting-image-files.md.
QSet<QString> referencedStrings(const std::vector<const Task *> &tasks);

} // namespace mirante::edits
