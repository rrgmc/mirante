#pragma once

#include "core/Entities.h"

#include <QDialog>

#include <memory>

namespace mirante {

class ConnectionEditor;

// Edits the connections of one profile, opened with Connections… in the
// profiles dialog. It shows a ConnectionEditor. OK is refused while a
// connection has no name or no valid base URL. See docs/ui.md#connections.
// Implemented in ConnectionsDialog.cpp.
class ConnectionsDialog : public QDialog
{
    Q_OBJECT

public:
    // profileName names the profile in the title, such as "Work".
    ConnectionsDialog(const Profile &profile, const QString &profileName, QWidget *parent = nullptr);
    ~ConnectionsDialog() override;

    // The connection kinds offered, from ItemTypeRegistry::connectionKinds().
    void setConnectionKinds(const QStringList &kinds);

    // The profile with the edited connections and defaults; every other field
    // as given.
    Profile value() const;

    ConnectionEditor *editor() const;   // for tests

    void accept() override;

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
