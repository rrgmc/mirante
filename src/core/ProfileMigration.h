#pragma once

#include "core/Entities.h"

#include <QJsonObject>
#include <QStringList>

#include <vector>

// Moves data from before decision 0050 into profiles: the connections of
// groups, and the module settings at the top of workspace.mirante. The Store
// runs it when it loads or reloads files, and writes the changed files once.
// See docs/decisions/0050-profiles-hold-logins-connections-and-settings.md.
// Implemented in ProfileMigration.cpp.
namespace mirante::profilemigration {

// The keys of a group file from before decision 0050. Group::fromJson keeps
// them in extra.
inline const QString kOldConnections = QStringLiteral("connections");
inline const QString kOldDefaultConnections = QStringLiteral("defaultConnections");

// Whether the group still holds connections from before decision 0050.
bool hasOldConnections(const Group &group);

// What a migration changed, so the Store can write those files.
struct Changes
{
    bool profiles = false;   // the profile list changed: write workspace.mirante
    QStringList groups;      // groups whose old keys were removed
    QStringList projects;    // projects given a connection override

    bool any() const { return profiles || !groups.isEmpty() || !projects.isEmpty(); }
};

// Moves each group's connections to the profile the group uses, or to
// Default if it names none in profiles. Groups are taken by their order, then
// by id, so the result is the same on every device.
//
// - A connection keeps its id, so items and project overrides that name it
//   still find it. A connection whose id a profile already has is skipped:
//   it was moved before, and running the migration again changes nothing.
// - A connection whose name the profile already uses gets the group's name
//   added, such as "gitlab.com (Personal)", so two groups' connections stay
//   apart.
// - A group default becomes the profile default of its kind if the profile
//   has none yet. If the profile has another one, the group's projects that
//   have no override for that kind get the group default as their override,
//   so their items keep the same connection.
// - A group whose old keys cannot be read is left as it is, and keeps them.
//
// profiles gets an empty Default first if it has none.
Changes moveConnections(std::vector<Group> &groups, std::vector<Project> &projects, std::vector<Profile> &profiles);

// Moves the module settings stored at the top of workspace.mirante before
// decision 0050 into Default. A screen that Default already has keeps
// Default's values. Returns whether Default changed. profiles gets an empty
// Default first if it has none.
bool moveModuleSettings(const QJsonObject &oldSettings, std::vector<Profile> &profiles);

} // namespace mirante::profilemigration
