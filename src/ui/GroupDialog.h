#pragma once

#include "core/Entities.h"

#include <QDialog>

#include <memory>

namespace mirante {

// Creates or edits a group: its name, its colour, its default web profile and
// its connections. OK is refused while the name is empty. See docs/ui.md#group-dialog.
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

    // The web profiles offered, from webprofiles::all(). Default is offered
    // even if missing. Only Default by default.
    void setWebProfiles(const std::vector<WebProfile> &profiles);

    // The colour shown as chosen: the group's own, or its palette colour if
    // it has none. A colour chosen in the dialog is stored; otherwise the
    // group's own is kept, so a group without one stays without one.
    QString colour() const;

    void setName(const QString &name);    // for tests
    void setWebProfile(const QString &id);   // for tests
    void setColour(const QString &colour);   // for tests; as if chosen in the dialog

    void accept() override;

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
