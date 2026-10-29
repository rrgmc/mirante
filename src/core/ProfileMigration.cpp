#include "core/ProfileMigration.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QDebug>
#include <QMap>
#include <QSet>

#include <algorithm>
#include <optional>

namespace mirante::profilemigration {

namespace {

const QString kId = QStringLiteral("id");
const QString kKind = QStringLiteral("kind");
const QString kName = QStringLiteral("name");
const QString kBaseUrl = QStringLiteral("baseUrl");
const QString kGeneral = QStringLiteral("general");

// The modules whose settings take the place of connections, and the
// connection kinds each read, in order of preference. These are the only
// connection kinds there ever were. See decision 0054.
struct ModuleKinds
{
    QString module;
    QStringList kinds;
};

const std::vector<ModuleKinds> &moduleKinds()
{
    static const std::vector<ModuleKinds> list{
        {QStringLiteral("gitlab"), {QStringLiteral("gitlab")}},
        {QStringLiteral("jira"), {QStringLiteral("jira.cloud"), QStringLiteral("jira.dc")}},
    };
    return list;
}

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

// The old keys of a group or profile, or nullopt if they cannot be read.
// Each connection is kept as its JSON object.
struct OldKeys
{
    std::vector<QJsonObject> connections;
    QMap<QString, QString> defaults;
};

std::optional<OldKeys> readOldKeys(const QJsonObject &extra)
{
    OldKeys keys;
    const QJsonValue connections = extra.value(kOldConnections);
    if (!connections.isUndefined()) {
        if (!connections.isArray())
            return std::nullopt;
        for (const QJsonValue &value : connections.toArray()) {
            const QJsonObject connection = value.toObject();
            if (!value.isObject() || connection.value(kId).toString().isEmpty())
                return std::nullopt;
            keys.connections.push_back(connection);
        }
    }
    const QJsonValue defaults = extra.value(kOldDefaultConnections);
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

bool isModuleKind(const QString &kind)
{
    return std::any_of(moduleKinds().begin(), moduleKinds().end(),
                       [&kind](const ModuleKinds &entry) { return entry.kinds.contains(kind); });
}

void logDropped(const QJsonObject &connection, const QString &profileId, const QString &reason)
{
    qInfo().noquote() << QStringLiteral("Dropped the connection \"%1\" (%2, %3) of profile %4: %5")
                                         .arg(connection.value(kName).toString(), connection.value(kKind).toString(),
                                              connection.value(kBaseUrl).toString(), profileId, reason);
}

// Moves connections into profile: the base URL of each module, and the
// connections of other kinds into its extra.
void apply(Profile &profile, const OldKeys &old)
{
    for (const ModuleKinds &entry : moduleKinds()) {
        std::vector<QJsonObject> candidates;
        for (const QJsonObject &connection : old.connections) {
            if (entry.kinds.contains(connection.value(kKind).toString()))
                candidates.push_back(connection);
        }
        if (candidates.empty())
            continue;

        // The default of the first kind that has one, else the first.
        std::optional<QJsonObject> winner;
        for (const QString &kind : entry.kinds) {
            const QString id = old.defaults.value(kind);
            for (const QJsonObject &connection : candidates) {
                if (!winner && !id.isEmpty() && connection.value(kId).toString() == id
                    && connection.value(kKind).toString() == kind)
                    winner = connection;
            }
        }
        if (!winner)
            winner = candidates.front();

        QJsonObject module = profile.moduleSettings.value(entry.module).toObject();
        QJsonObject general = module.value(kGeneral).toObject();
        const QString existing = general.value(kBaseUrl).toString();
        const QString baseUrl = winner->value(kBaseUrl).toString();
        const bool take = existing.isEmpty() && !baseUrl.isEmpty();
        if (take) {
            general.insert(kBaseUrl, baseUrl);
            module.insert(kGeneral, general);
            profile.moduleSettings.insert(entry.module, module);
        }
        for (const QJsonObject &connection : candidates) {
            if ((take && connection == *winner) || connection.value(kBaseUrl).toString() == existing)
                continue;
            logDropped(connection, profile.id,
                       take ? QStringLiteral("the profile keeps one server per module")
                            : QStringLiteral("the profile already has a base URL"));
        }
    }

    // Other kinds stay, under the old keys, unless the profile has them.
    QJsonArray kept = profile.extra.value(kOldConnections).toArray();
    QSet<QString> ids;
    for (const QJsonValue &value : std::as_const(kept))
        ids.insert(value.toObject().value(kId).toString());
    QJsonObject keptDefaults = profile.extra.value(kOldDefaultConnections).toObject();
    for (const QJsonObject &connection : old.connections) {
        const QString kind = connection.value(kKind).toString();
        if (isModuleKind(kind) || ids.contains(connection.value(kId).toString()))
            continue;
        kept.append(connection);
        ids.insert(connection.value(kId).toString());
    }
    for (auto it = old.defaults.begin(); it != old.defaults.end(); ++it) {
        if (!isModuleKind(it.key()) && !keptDefaults.contains(it.key()))
            keptDefaults.insert(it.key(), it.value());
    }
    profile.extra.remove(kOldConnections);
    profile.extra.remove(kOldDefaultConnections);
    if (!kept.isEmpty())
        profile.extra.insert(kOldConnections, kept);
    if (!keptDefaults.isEmpty())
        profile.extra.insert(kOldDefaultConnections, keptDefaults);
}

} // namespace

bool hasOldConnections(const Group &group)
{
    return group.extra.contains(kOldConnections) || group.extra.contains(kOldDefaultConnections);
}

bool hasOldConnections(const Profile &profile)
{
    const std::optional<OldKeys> old = readOldKeys(profile.extra);
    if (!old)
        return false;
    return std::any_of(old->connections.begin(), old->connections.end(), [](const QJsonObject &connection) {
        return isModuleKind(connection.value(kKind).toString());
    });
}

Changes moveConnections(std::vector<Group> &groups, std::vector<Profile> &profiles)
{
    Changes changes;
    defaultIndex(profiles);

    for (Profile &profile : profiles) {
        if (!hasOldConnections(profile))
            continue;
        const std::optional<OldKeys> old = readOldKeys(profile.extra);
        const Profile before = profile;
        profile.extra.remove(kOldConnections);
        profile.extra.remove(kOldDefaultConnections);
        apply(profile, *old);
        changes.profiles = changes.profiles || profile != before;
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
        const std::optional<OldKeys> old = readOldKeys(group->extra);
        if (!old)
            continue;
        Profile &target = targetOf(*group, profiles);
        const Profile before = target;
        apply(target, *old);
        changes.profiles = changes.profiles || target != before;
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
