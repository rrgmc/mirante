#pragma once

#include "core/LayoutTree.h"

#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QStringList>

#include <optional>
#include <vector>

// The entities of docs/data-model.md, as plain values with JSON conversion.
// Implemented in Entities.cpp.
//
// Conventions for every entity:
// - Ids are QStrings holding UUIDs, so write-back is exact.
// - fromJson() returns nullopt and sets *error for data that cannot be used,
//   such as a missing id. A field of the wrong type falls back to its default,
//   and the original value is kept in extra.
// - fromJson() moves every key it does not know into extra. toJson() writes
//   every known field and then extra, so nothing is lost.
// - schemaVersion is read and written by the Store, not by the entities.
// - operator== compares every field, including extra. The Model uses it to
//   skip changes that change nothing.

namespace mirante {

enum class Priority { Urgent = 0, High = 1, Normal = 2, Low = 3 };

enum class TaskState { Active, Done, Archived };

struct Folder
{
    QString id;
    QString name;
    QString remoteUrl;
    bool main = false;
    QJsonObject extra;

    static std::optional<Folder> fromJson(const QJsonObject &json, QString *error = nullptr);
    QJsonObject toJson() const;
};

// A profile of the workspace, listed in workspace.mirante: web logins (kept
// on each device) and module settings, such as the server of each module.
// Tokens are kept in the keychain. A workspace always has at least one, and
// one is its default. See docs/data-model.md#profile and decisions 0050, 0054
// and 0057.
struct Profile
{
    QString id;
    QString name;
    // The values of the modules' settings screens: an object of module ids,
    // each an object of screen ids, each the values of that screen.
    QJsonObject moduleSettings;
    QJsonObject extra;

    // Whether the profile holds anything besides its id and name.
    bool hasData() const;

    static std::optional<Profile> fromJson(const QJsonObject &json, QString *error = nullptr);
    QJsonObject toJson() const;
};

// The id of the profile a workspace gets when it has none, called "Default".
// Workspaces from before decision 0057 always have it, as their default.
inline const QString kDefaultProfile = QStringLiteral("default");

struct Group
{
    QString id;
    QString name;
    int order = 0;
    QString profile;                             // id of the default profile; empty for Default
    QString colour;                              // "#rrggbb"; empty or invalid uses a palette colour
    QJsonObject extra;

    static std::optional<Group> fromJson(const QJsonObject &json, QString *error = nullptr);
    QJsonObject toJson() const;
};

struct Project
{
    QString id;
    QString groupId;
    QString name;
    Priority priority = Priority::Normal;
    int order = 0;
    std::vector<Folder> folders;
    QJsonObject moduleSettings;                  // one object per module id; opaque to the core
    QString trashedAt;                           // ISO 8601 UTC; empty if not in the trash
    QJsonObject extra;

    bool isTrashed() const { return !trashedAt.isEmpty(); }

    static std::optional<Project> fromJson(const QJsonObject &json, QString *error = nullptr);
    QJsonObject toJson() const;
};

struct Item
{
    QString id;
    QString type;               // item type id, such as "gitlab.mr.summary"
    QString name;
    QJsonObject config;         // fields of the item type; opaque to the core
    QString profile;            // id of a profile overriding the group's; empty if not set
    bool watch = false;         // counts on the tray and notifies; decision 0087
    QStringList pageKeys;       // keys its web page keeps, in PortableText; decision 0088
    QJsonObject extra;

    static std::optional<Item> fromJson(const QJsonObject &json, QString *error = nullptr);
    QJsonObject toJson() const;
};

// A task dashboard, and the base of a root dashboard.
struct Dashboard
{
    QString id;
    QString name;
    LayoutNode layout;
    QJsonObject extra;

    static std::optional<Dashboard> fromJson(const QJsonObject &json, QString *error = nullptr);
    QJsonObject toJson() const;
};

// A root dashboard: a dashboard outside the hierarchy, with a tab bar position.
struct RootDashboard : Dashboard
{
    int order = 0;

    static std::optional<RootDashboard> fromJson(const QJsonObject &json, QString *error = nullptr);
    QJsonObject toJson() const;
};

struct Task
{
    QString id;
    QString projectId;
    QString title;
    TaskState state = TaskState::Active;
    Priority priority = Priority::Normal;
    int order = 0;
    std::vector<Item> items;
    std::vector<Dashboard> dashboards;
    QString defaultDashboardId;
    QString trashedAt;          // ISO 8601 UTC; empty if not in the trash
    QJsonObject extra;

    bool isTrashed() const { return !trashedAt.isEmpty(); }

    static std::optional<Task> fromJson(const QJsonObject &json, QString *error = nullptr);
    QJsonObject toJson() const;
};

bool operator==(const Folder &a, const Folder &b);
bool operator==(const Profile &a, const Profile &b);
bool operator==(const Group &a, const Group &b);
bool operator==(const Project &a, const Project &b);
bool operator==(const Item &a, const Item &b);
bool operator==(const Dashboard &a, const Dashboard &b);
bool operator==(const RootDashboard &a, const RootDashboard &b);
bool operator==(const Task &a, const Task &b);

inline bool operator!=(const Folder &a, const Folder &b) { return !(a == b); }
inline bool operator!=(const Profile &a, const Profile &b) { return !(a == b); }
inline bool operator!=(const Group &a, const Group &b) { return !(a == b); }
inline bool operator!=(const Project &a, const Project &b) { return !(a == b); }
inline bool operator!=(const Item &a, const Item &b) { return !(a == b); }
inline bool operator!=(const Dashboard &a, const Dashboard &b) { return !(a == b); }
inline bool operator!=(const RootDashboard &a, const RootDashboard &b) { return !(a == b); }
inline bool operator!=(const Task &a, const Task &b) { return !(a == b); }

} // namespace mirante
