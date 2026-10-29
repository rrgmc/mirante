#include "core/Connections.h"

#include "core/Entities.h"
#include "core/Model.h"

namespace mirante::connections {

namespace {

// The connection of group with id, if its kind is kind.
const Connection *find(const Group &group, const QString &id, const QString &kind)
{
    if (id.isEmpty())
        return nullptr;
    for (const Connection &connection : group.connections) {
        if (connection.id == id)
            return connection.kind == kind ? &connection : nullptr;
    }
    return nullptr;
}

} // namespace

const Connection *resolve(const Model &model, const QString &taskId, const Item &item, const QStringList &kinds)
{
    const Task *task = model.task(taskId);
    const Project *project = task ? model.project(task->projectId) : nullptr;
    const Group *group = project ? model.group(project->groupId) : nullptr;
    if (!group)
        return nullptr;

    // The item's own choice, then the project's override, then the group
    // default. At each step the kinds are tried in order.
    for (const QString &kind : kinds) {
        if (const Connection *connection = find(*group, item.connectionId, kind))
            return connection;
    }
    for (const QString &kind : kinds) {
        if (const Connection *connection = find(*group, project->connectionOverrides.value(kind), kind))
            return connection;
    }
    for (const QString &kind : kinds) {
        if (const Connection *connection = find(*group, group->defaultConnections.value(kind), kind))
            return connection;
    }
    return nullptr;
}

} // namespace mirante::connections
