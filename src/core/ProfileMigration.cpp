#include "core/ProfileMigration.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QMap>
#include <QSet>

#include <algorithm>
#include <optional>

namespace mirante::profilemigration {

namespace {

// The index of Default in profiles, added first if missing.
std::size_t defaultIndex(std::vector<Profile> &profiles)
{
    for (std::size_t i = 0; i < profiles.size(); ++i) {
        if (profiles[i].id == kDefaultProfile)
            return i;
    }
    Profile standard;
    standard.id = kDefaultProfile;
    profiles.insert(profiles.begin(), standard);
    return 0;
}

Profile &targetOf(const Group &group, std::vector<Profile> &profiles)
{
    const std::size_t standard = defaultIndex(profiles);
    if (!group.profile.isEmpty()) {
        for (Profile &profile : profiles) {
            if (profile.id == group.profile)
                return profile;
        }
    }
    return profiles[standard];
}

bool nameUsed(const Profile &profile, const QString &name)
{
    return std::any_of(profile.connections.begin(), profile.connections.end(), [&name](const Connection &c) {
        return c.name.compare(name, Qt::CaseInsensitive) == 0;
    });
}

// name, or name with the group's name added if the profile uses it already.
QString distinctName(const Profile &profile, const QString &name, const QString &groupName)
{
    if (!nameUsed(profile, name))
        return name;
    const QString base = QStringLiteral("%1 (%2)").arg(name, groupName);
    QString candidate = base;
    for (int n = 2; nameUsed(profile, candidate); ++n)
        candidate = QStringLiteral("%1 (%2 %3)").arg(name, groupName).arg(n);
    return candidate;
}

const Connection *connectionOf(const Profile &profile, const QString &id, const QString &kind)
{
    for (const Connection &connection : profile.connections) {
        if (connection.id == id)
            return connection.kind == kind ? &connection : nullptr;
    }
    return nullptr;
}

// The old keys of a group, or nullopt if they cannot be read.
struct OldKeys
{
    std::vector<Connection> connections;
    QMap<QString, QString> defaults;
};

std::optional<OldKeys> readOldKeys(const Group &group)
{
    OldKeys keys;
    const QJsonValue connections = group.extra.value(kOldConnections);
    if (!connections.isUndefined()) {
        if (!connections.isArray())
            return std::nullopt;
        for (const QJsonValue &value : connections.toArray()) {
            if (!value.isObject())
                return std::nullopt;
            std::optional<Connection> connection = Connection::fromJson(value.toObject());
            if (!connection)
                return std::nullopt;
            keys.connections.push_back(std::move(*connection));
        }
    }
    const QJsonValue defaults = group.extra.value(kOldDefaultConnections);
    if (!defaults.isUndefined()) {
        if (!defaults.isObject())
            return std::nullopt;
        const QJsonObject object = defaults.toObject();
        for (auto it = object.begin(); it != object.end(); ++it) {
            if (!it.value().isString())
                return std::nullopt;
            keys.defaults.insert(it.key(), it.value().toString());
        }
    }
    return keys;
}

} // namespace

bool hasOldConnections(const Group &group)
{
    return group.extra.contains(kOldConnections) || group.extra.contains(kOldDefaultConnections);
}

Changes moveConnections(std::vector<Group> &groups, std::vector<Project> &projects, std::vector<Profile> &profiles)
{
    Changes changes;
    defaultIndex(profiles);

    QSet<QString> known;
    for (const Profile &profile : profiles) {
        for (const Connection &connection : profile.connections)
            known.insert(connection.id);
    }

    std::vector<Group *> ordered;
    for (Group &group : groups) {
        if (hasOldConnections(group))
            ordered.push_back(&group);
    }
    std::sort(ordered.begin(), ordered.end(), [](const Group *a, const Group *b) {
        return a->order != b->order ? a->order < b->order : a->id < b->id;
    });

    for (Group *group : ordered) {
        const std::optional<OldKeys> old = readOldKeys(*group);
        if (!old)
            continue;
        Profile &target = targetOf(*group, profiles);

        for (Connection connection : old->connections) {
            if (known.contains(connection.id))
                continue;
            connection.name = distinctName(target, connection.name, group->name);
            target.connections.push_back(connection);
            known.insert(connection.id);
            changes.profiles = true;
        }

        for (auto it = old->defaults.begin(); it != old->defaults.end(); ++it) {
            const QString &kind = it.key();
            const QString &id = it.value();
            if (!connectionOf(target, id, kind))
                continue;   // a default that names no connection of its kind
            const QString current = target.defaultConnections.value(kind);
            if (current == id)
                continue;
            if (!connectionOf(target, current, kind)) {
                target.defaultConnections.insert(kind, id);
                changes.profiles = true;
                continue;
            }
            // The profile has another default: the group's projects keep
            // theirs through an override.
            for (Project &project : projects) {
                if (project.groupId != group->id || project.connectionOverrides.contains(kind))
                    continue;
                project.connectionOverrides.insert(kind, id);
                if (!changes.projects.contains(project.id))
                    changes.projects << project.id;
            }
        }

        group->extra.remove(kOldConnections);
        group->extra.remove(kOldDefaultConnections);
        changes.groups << group->id;
    }
    return changes;
}

bool moveModuleSettings(const QJsonObject &oldSettings, std::vector<Profile> &profiles)
{
    Profile &standard = profiles[defaultIndex(profiles)];
    bool changed = false;
    for (auto module = oldSettings.begin(); module != oldSettings.end(); ++module) {
        const QJsonValue existing = standard.moduleSettings.value(module.key());
        if (existing.isUndefined()) {
            standard.moduleSettings.insert(module.key(), module.value());
            changed = true;
            continue;
        }
        if (!existing.isObject() || !module.value().isObject())
            continue;   // Default's own value wins
        QJsonObject screens = existing.toObject();
        const QJsonObject oldScreens = module.value().toObject();
        bool added = false;
        for (auto screen = oldScreens.begin(); screen != oldScreens.end(); ++screen) {
            if (!screens.contains(screen.key())) {
                screens.insert(screen.key(), screen.value());
                added = true;
            }
        }
        if (added) {
            standard.moduleSettings.insert(module.key(), screens);
            changed = true;
        }
    }
    return changed;
}

} // namespace mirante::profilemigration
