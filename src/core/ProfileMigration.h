#pragma once

#include "core/Entities.h"

#include <QJsonObject>
#include <QStringList>

#include <vector>

// Moves data from before decisions 0050 and 0054 into profiles: connections,
// first kept in groups and then in profiles, become the base URL in the
// GitLab and Jira module settings of a profile; and the module settings at
// the top of workspace.mirante move to Default. The Store runs it when it
// loads or reloads files, and writes the changed files once. See
// docs/decisions/0054-module-settings-replace-connections.md.
// Implemented in ProfileMigration.cpp.
namespace mirante::profilemigration {

// The keys that held connections before decision 0054, in group files and in
// the entries of workspace.mirante's profile list. Group::fromJson and
// Profile::fromJson keep them in extra.
inline const QString kOldConnections = QStringLiteral("connections");
inline const QString kOldDefaultConnections = QStringLiteral("defaultConnections");

// Whether the group or the profile still holds connections.
bool hasOldConnections(const Group &group);
bool hasOldConnections(const Profile &profile);

// What a migration changed, so the Store can write those files.
struct Changes
{
    bool profiles = false;   // the profile list changed: write workspace.mirante
    QStringList groups;      // groups whose old keys were removed

    bool any() const { return profiles || !groups.isEmpty(); }
};

// Turns connections into module settings.
//
// - First each profile's own connections, then each group's, by the group's
//   order, then id. A group's go to the profile the group uses, or to
//   Default if it names none in profiles.
// - A connection of kind "gitlab" gives the base URL of the profile's GitLab
//   settings, gitlab.general.baseUrl. One of kind "jira.cloud" or "jira.dc"
//   gives jira.general.baseUrl. The default connection of the kind wins, else
//   the first; a base URL the profile already has wins over all. The others
//   are dropped, each with a line in the log.
// - Connections of other kinds, which no module of this version reads, are
//   kept in the profile's extra, under the old keys.
// - A group whose old keys cannot be read is left as it is, and keeps them.
//
// Running it again changes nothing. profiles gets an empty Default first if
// it has none.
Changes moveConnections(std::vector<Group> &groups, std::vector<Profile> &profiles);

// Moves the module settings stored at the top of workspace.mirante before
// decision 0050 into Default. A screen that Default already has keeps
// Default's values. Returns whether Default changed. profiles gets an empty
// Default first if it has none.
bool moveModuleSettings(const QJsonObject &oldSettings, std::vector<Profile> &profiles);

} // namespace mirante::profilemigration
