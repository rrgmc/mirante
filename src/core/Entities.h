#pragma once

#include "core/LayoutTree.h"

#include <QJsonObject>
#include <QMap>
#include <QString>

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

struct Connection
{
    QString id;
    QString kind;               // such as "gitlab" or "jira.cloud"
    QString name;
    QString baseUrl;
    QJsonObject fields;         // declared by the connection kind; opaque to the core
    QJsonObject extra;

    static std::optional<Connection> fromJson(const QJsonObject &json, QString *error = nullptr);
    QJsonObject toJson() const;
};

struct Group
{
    QString id;
    QString name;
    int order = 0;
    std::vector<Connection> connections;
    QMap<QString, QString> defaultConnections;   // connection kind to connection id
    QJsonObject profileOverrides;                // site host to "shared"
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
    QMap<QString, QString> connectionOverrides;  // connection kind to connection id
    QJsonObject extra;

    static std::optional<Project> fromJson(const QJsonObject &json, QString *error = nullptr);
    QJsonObject toJson() const;
};

struct Item
{
    QString id;
    QString type;               // item type id, such as "gitlab.mr.summary"
    QString name;
    QJsonObject config;         // fields of the item type; opaque to the core
    QString connectionId;       // empty if not set
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
    bool isDefault = false;
    TaskState state = TaskState::Active;
    Priority priority = Priority::Normal;
    int order = 0;
    std::vector<Item> items;
    std::vector<Dashboard> dashboards;
    QString defaultDashboardId;
    QJsonObject extra;

    static std::optional<Task> fromJson(const QJsonObject &json, QString *error = nullptr);
    QJsonObject toJson() const;
};

} // namespace mirante
