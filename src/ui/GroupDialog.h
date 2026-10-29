#pragma once

#include "core/Entities.h"

#include <QDialog>

#include <memory>

namespace mirante {

// Creates or edits a group: its name, its connections and the sites in the
// shared profile. OK is refused while the name is empty. See docs/ui.md#group-dialog.
// Implemented in GroupDialog.cpp.
class GroupDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GroupDialog(const Group &group, QWidget *parent = nullptr);
    ~GroupDialog() override;

    // The group with the edited fields; every other field as given.
    Group value() const;

    // The connection kinds offered, from ItemTypeRegistry::connectionKinds().
    // Connections of other kinds are read-only. None by default.
    void setConnectionKinds(const QStringList &kinds);

    void setName(const QString &name);    // for tests

    void accept() override;

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
