#pragma once

#include <QString>
#include <QStringList>

namespace mirante {

class Model;
struct Connection;
struct Item;

namespace connections {

// The connection for an item, by the resolution order in
// docs/data-model.md#connection: the item's connectionId, the project's
// override, the group default. Only connections of the item's group, of one of
// kinds, count. kinds are tried in order at each step. Null if none is found.
// Implemented in Connections.cpp.
const Connection *resolve(const Model &model, const QString &taskId, const Item &item, const QStringList &kinds);

} // namespace connections
} // namespace mirante
