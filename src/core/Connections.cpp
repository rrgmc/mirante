#include "core/Connections.h"

#include "core/Entities.h"
#include "core/Model.h"

namespace mirante::connections {

// Stub from M4 T0. T4 implements it.
const Connection *resolve(const Model &model, const QString &taskId, const Item &item, const QStringList &kinds)
{
    Q_UNUSED(model)
    Q_UNUSED(taskId)
    Q_UNUSED(item)
    Q_UNUSED(kinds)
    return nullptr;
}

} // namespace mirante::connections
