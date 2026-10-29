#include "core/Edits.h"

#include <QUuid>

// Stubs written by T0 of the M3 plan. T1 implements them.

namespace mirante::edits {

QString newId() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }

Task newTask(const QString &projectId, const QString &title)
{
    Task task;
    task.id = newId();
    task.projectId = projectId;
    task.title = title;
    return task;
}

Task addItem(Task task, const Item &) { return task; }
Task removeItem(Task task, const QString &) { return task; }
Task addDashboard(Task task, const Dashboard &) { return task; }
Task removeDashboard(Task task, const QString &) { return task; }
Task setDefaultDashboard(Task task, const QString &) { return task; }

std::vector<Project> moveProject(std::vector<Project>, const QString &, Priority, int) { return {}; }
std::vector<Task> moveTask(std::vector<Task>, const QString &, Priority, int) { return {}; }
std::vector<Group> moveGroup(std::vector<Group>, const QString &, int) { return {}; }

QSet<QString> referencedStrings(const std::vector<const Task *> &) { return {}; }

} // namespace mirante::edits
