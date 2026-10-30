#pragma once

#include "core/Entities.h"

#include <QDialog>

#include <memory>

namespace mirante {

// Creates or edits a group: its name, its colour and its default profile. OK
// is refused while the name is empty. See docs/ui.md#group-dialog.
// Implemented in GroupDialog.cpp.
class GroupDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GroupDialog(const Group &group, QWidget *parent = nullptr);
    ~GroupDialog() override;

    // The group with the edited fields; every other field as given.
    Group value() const;

    // The profiles offered, from profiles::all(). The first entry is "The
    // default profile (<defaultName>)", stored as no profile, so the group
    // follows the workspace's default. Only that entry by default.
    void setProfiles(const std::vector<Profile> &profiles, const QString &defaultName);

    // The colour shown as chosen: the group's own, or its palette colour if
    // it has none. A colour chosen in the dialog is stored; otherwise the
    // group's own is kept, so a group without one stays without one.
    QString colour() const;

    // Offers "Create a new profile for this group", unticked, with a profile
    // name that follows the group's name until it is edited. While ticked,
    // the profile list is disabled. EntityActions offers it for a new group
    // when other groups exist. existingNames are refused, ignoring case.
    void offerNewProfile(const QStringList &existingNames);
    // Whether the new profile was asked for, and its name, trimmed.
    bool createsProfile() const;
    QString newProfileName() const;

    void setName(const QString &name);    // for tests
    void setCreatesProfile(bool on);           // for tests
    void setNewProfileName(const QString &name);  // for tests
    void setProfile(const QString &id);      // for tests
    void setColour(const QString &colour);   // for tests; as if chosen in the dialog

    void accept() override;

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
