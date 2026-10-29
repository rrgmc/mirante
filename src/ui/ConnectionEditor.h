#pragma once

#include "core/Entities.h"

#include <QMap>
#include <QWidget>

#include <memory>
#include <vector>

class QTableWidget;

namespace mirante {

// Edits a list of connections and the default of each kind: a table with a
// kind, a name, a base URL and a Default check box per row, with Add
// connection and Remove connection. The first connection of a kind becomes
// its default. Connections of a kind that is not offered are read-only and
// kept unchanged. See docs/ui.md#connections. Implemented in ConnectionEditor.cpp.
class ConnectionEditor : public QWidget
{
    Q_OBJECT

public:
    ConnectionEditor(const std::vector<Connection> &connections, const QMap<QString, QString> &defaults,
                     QWidget *parent = nullptr);
    ~ConnectionEditor() override;

    // The connection kinds offered, from ItemTypeRegistry::connectionKinds().
    // Connections of other kinds are read-only. None by default.
    void setConnectionKinds(const QStringList &kinds);

    std::vector<Connection> connections() const;
    QMap<QString, QString> defaults() const;   // connection kind to connection id

    // Why the connections cannot be saved, or an empty string: an editable
    // connection without a name, or without a base URL that starts with
    // https:// or http://.
    QString problem() const;

    QTableWidget *table() const;   // for tests and focus
    void addConnection();          // as the button does
    void removeCurrent();          // as the button does

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
