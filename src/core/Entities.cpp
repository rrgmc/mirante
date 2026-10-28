#include "core/Entities.h"

#include <QJsonArray>
#include <QJsonValue>

#include <cmath>
#include <limits>
#include <utility>

namespace mirante {

namespace {

// Reads the known fields of one JSON object. It starts with a copy of the
// object as extra and removes each key it reads successfully, so unknown keys
// and known keys of the wrong type stay in extra.
class Reader
{
public:
    explicit Reader(const QJsonObject &json) : m_json(json), m_extra(json) {}

    QJsonObject extra() const { return m_extra; }

    // A string that must be present and not empty.
    bool required(const QString &key, QString *out, QString *error)
    {
        const QJsonValue value = m_json.value(key);
        if (!value.isString() || value.toString().isEmpty()) {
            setError(error, QStringLiteral("missing or invalid \"%1\"").arg(key));
            return false;
        }
        *out = value.toString();
        m_extra.remove(key);
        return true;
    }

    void string(const QString &key, QString *out)
    {
        if (const auto value = take(key, [](const QJsonValue &v) { return v.isString(); }))
            *out = value->toString();
    }

    void boolean(const QString &key, bool *out)
    {
        if (const auto value = take(key, [](const QJsonValue &v) { return v.isBool(); }))
            *out = value->toBool();
    }

    void integer(const QString &key, int *out)
    {
        if (const auto value = take(key, isInteger))
            *out = static_cast<int>(value->toDouble());
    }

    void object(const QString &key, QJsonObject *out)
    {
        if (const auto value = take(key, [](const QJsonValue &v) { return v.isObject(); }))
            *out = value->toObject();
    }

    // An object whose values are all strings.
    void stringMap(const QString &key, QMap<QString, QString> *out)
    {
        const auto isStringMap = [](const QJsonValue &v) {
            if (!v.isObject())
                return false;
            const QJsonObject object = v.toObject();
            for (auto it = object.begin(); it != object.end(); ++it) {
                if (!it.value().isString())
                    return false;
            }
            return true;
        };
        if (const auto value = take(key, isStringMap)) {
            const QJsonObject object = value->toObject();
            for (auto it = object.begin(); it != object.end(); ++it)
                out->insert(it.key(), it.value().toString());
        }
    }

    void priority(const QString &key, Priority *out)
    {
        const auto isPriority = [](const QJsonValue &v) {
            return isInteger(v) && v.toDouble() >= 0 && v.toDouble() <= 3;
        };
        if (const auto value = take(key, isPriority))
            *out = static_cast<Priority>(static_cast<int>(value->toDouble()));
    }

    void state(const QString &key, TaskState *out)
    {
        const auto isState = [](const QJsonValue &v) {
            const QString s = v.toString();
            return v.isString()
                && (s == QLatin1String("active") || s == QLatin1String("done")
                    || s == QLatin1String("archived"));
        };
        if (const auto value = take(key, isState)) {
            const QString s = value->toString();
            *out = s == QLatin1String("done")       ? TaskState::Done
                 : s == QLatin1String("archived")   ? TaskState::Archived
                                                    : TaskState::Active;
        }
    }

    // A list of entities. A value that is not an array falls back to an empty
    // list. An element that cannot be used makes the whole entity unusable.
    template <typename T>
    bool list(const QString &key, std::vector<T> *out, QString *error)
    {
        const auto value = take(key, [](const QJsonValue &v) { return v.isArray(); });
        if (!value)
            return true;
        const QJsonArray array = value->toArray();
        for (qsizetype i = 0; i < array.size(); ++i) {
            const QString where = QStringLiteral("%1[%2]: ").arg(key).arg(i);
            if (!array.at(i).isObject()) {
                setError(error, where + QStringLiteral("not an object"));
                return false;
            }
            QString elementError;
            auto element = T::fromJson(array.at(i).toObject(), &elementError);
            if (!element) {
                setError(error, where + elementError);
                return false;
            }
            out->push_back(std::move(*element));
        }
        return true;
    }

    // Removes key from extra and returns its value if it is present and
    // accepted. Otherwise the key, if present, stays in extra.
    template <typename Accept>
    std::optional<QJsonValue> take(const QString &key, Accept accept)
    {
        const auto it = m_json.constFind(key);
        if (it == m_json.constEnd() || !accept(it.value()))
            return std::nullopt;
        m_extra.remove(key);
        return it.value();
    }

    static bool isInteger(const QJsonValue &v)
    {
        if (!v.isDouble())
            return false;
        const double d = v.toDouble();
        return std::floor(d) == d && d >= std::numeric_limits<int>::min()
            && d <= std::numeric_limits<int>::max();
    }

    static void setError(QString *error, const QString &text)
    {
        if (error)
            *error = text;
    }

private:
    QJsonObject m_json;
    QJsonObject m_extra;
};

// Writes extra over the known fields, so a value kept in extra wins.
QJsonObject withExtra(QJsonObject json, const QJsonObject &extra)
{
    for (auto it = extra.begin(); it != extra.end(); ++it)
        json.insert(it.key(), it.value());
    return json;
}

template <typename T>
QJsonArray listToJson(const std::vector<T> &list)
{
    QJsonArray array;
    for (const T &element : list)
        array.append(element.toJson());
    return array;
}

QJsonObject mapToJson(const QMap<QString, QString> &map)
{
    QJsonObject json;
    for (auto it = map.begin(); it != map.end(); ++it)
        json.insert(it.key(), it.value());
    return json;
}

QString stateToString(TaskState state)
{
    switch (state) {
    case TaskState::Done: return QStringLiteral("done");
    case TaskState::Archived: return QStringLiteral("archived");
    case TaskState::Active: break;
    }
    return QStringLiteral("active");
}

} // namespace

std::optional<Folder> Folder::fromJson(const QJsonObject &json, QString *error)
{
    Reader r(json);
    Folder folder;
    if (!r.required(QStringLiteral("id"), &folder.id, error))
        return std::nullopt;
    r.string(QStringLiteral("name"), &folder.name);
    r.string(QStringLiteral("remoteUrl"), &folder.remoteUrl);
    r.boolean(QStringLiteral("main"), &folder.main);
    folder.extra = r.extra();
    return folder;
}

QJsonObject Folder::toJson() const
{
    QJsonObject json{
        {QStringLiteral("id"), id},
        {QStringLiteral("name"), name},
        {QStringLiteral("main"), main},
    };
    if (!remoteUrl.isEmpty())
        json.insert(QStringLiteral("remoteUrl"), remoteUrl);
    return withExtra(json, extra);
}

std::optional<Connection> Connection::fromJson(const QJsonObject &json, QString *error)
{
    Reader r(json);
    Connection connection;
    if (!r.required(QStringLiteral("id"), &connection.id, error))
        return std::nullopt;
    r.string(QStringLiteral("kind"), &connection.kind);
    r.string(QStringLiteral("name"), &connection.name);
    r.string(QStringLiteral("baseUrl"), &connection.baseUrl);
    r.object(QStringLiteral("fields"), &connection.fields);
    connection.extra = r.extra();
    return connection;
}

QJsonObject Connection::toJson() const
{
    return withExtra({
        {QStringLiteral("id"), id},
        {QStringLiteral("kind"), kind},
        {QStringLiteral("name"), name},
        {QStringLiteral("baseUrl"), baseUrl},
        {QStringLiteral("fields"), fields},
    }, extra);
}

std::optional<Group> Group::fromJson(const QJsonObject &json, QString *error)
{
    Reader r(json);
    Group group;
    if (!r.required(QStringLiteral("id"), &group.id, error))
        return std::nullopt;
    r.string(QStringLiteral("name"), &group.name);
    r.integer(QStringLiteral("order"), &group.order);
    if (!r.list(QStringLiteral("connections"), &group.connections, error))
        return std::nullopt;
    r.stringMap(QStringLiteral("defaultConnections"), &group.defaultConnections);
    r.object(QStringLiteral("profileOverrides"), &group.profileOverrides);
    group.extra = r.extra();
    return group;
}

QJsonObject Group::toJson() const
{
    return withExtra({
        {QStringLiteral("id"), id},
        {QStringLiteral("name"), name},
        {QStringLiteral("order"), order},
        {QStringLiteral("connections"), listToJson(connections)},
        {QStringLiteral("defaultConnections"), mapToJson(defaultConnections)},
        {QStringLiteral("profileOverrides"), profileOverrides},
    }, extra);
}

std::optional<Project> Project::fromJson(const QJsonObject &json, QString *error)
{
    Reader r(json);
    Project project;
    if (!r.required(QStringLiteral("id"), &project.id, error)
        || !r.required(QStringLiteral("groupId"), &project.groupId, error))
        return std::nullopt;
    r.string(QStringLiteral("name"), &project.name);
    r.priority(QStringLiteral("priority"), &project.priority);
    r.integer(QStringLiteral("order"), &project.order);
    if (!r.list(QStringLiteral("folders"), &project.folders, error))
        return std::nullopt;
    r.object(QStringLiteral("moduleSettings"), &project.moduleSettings);
    r.stringMap(QStringLiteral("connectionOverrides"), &project.connectionOverrides);
    project.extra = r.extra();
    return project;
}

QJsonObject Project::toJson() const
{
    return withExtra({
        {QStringLiteral("id"), id},
        {QStringLiteral("groupId"), groupId},
        {QStringLiteral("name"), name},
        {QStringLiteral("priority"), static_cast<int>(priority)},
        {QStringLiteral("order"), order},
        {QStringLiteral("folders"), listToJson(folders)},
        {QStringLiteral("moduleSettings"), moduleSettings},
        {QStringLiteral("connectionOverrides"), mapToJson(connectionOverrides)},
    }, extra);
}

std::optional<Item> Item::fromJson(const QJsonObject &json, QString *error)
{
    Reader r(json);
    Item item;
    if (!r.required(QStringLiteral("id"), &item.id, error)
        || !r.required(QStringLiteral("type"), &item.type, error))
        return std::nullopt;
    r.string(QStringLiteral("name"), &item.name);
    r.object(QStringLiteral("config"), &item.config);
    r.string(QStringLiteral("connectionId"), &item.connectionId);
    item.extra = r.extra();
    return item;
}

QJsonObject Item::toJson() const
{
    QJsonObject json{
        {QStringLiteral("id"), id},
        {QStringLiteral("type"), type},
        {QStringLiteral("name"), name},
        {QStringLiteral("config"), config},
    };
    if (!connectionId.isEmpty())
        json.insert(QStringLiteral("connectionId"), connectionId);
    return withExtra(json, extra);
}

std::optional<Dashboard> Dashboard::fromJson(const QJsonObject &json, QString *error)
{
    Reader r(json);
    Dashboard dashboard;
    if (!r.required(QStringLiteral("id"), &dashboard.id, error))
        return std::nullopt;
    r.string(QStringLiteral("name"), &dashboard.name);
    // A missing or unusable layout is one empty cell, the default LayoutNode.
    QJsonObject layout;
    r.object(QStringLiteral("layout"), &layout);
    if (!layout.isEmpty())
        dashboard.layout = layoutFromJson(layout);
    dashboard.extra = r.extra();
    return dashboard;
}

QJsonObject Dashboard::toJson() const
{
    return withExtra({
        {QStringLiteral("id"), id},
        {QStringLiteral("name"), name},
        {QStringLiteral("layout"), layoutToJson(layout)},
    }, extra);
}

std::optional<RootDashboard> RootDashboard::fromJson(const QJsonObject &json, QString *error)
{
    const auto base = Dashboard::fromJson(json, error);
    if (!base)
        return std::nullopt;
    RootDashboard dashboard;
    static_cast<Dashboard &>(dashboard) = *base;
    // order is still in extra, because Dashboard does not know it.
    Reader r(base->extra);
    r.integer(QStringLiteral("order"), &dashboard.order);
    dashboard.extra = r.extra();
    return dashboard;
}

QJsonObject RootDashboard::toJson() const
{
    QJsonObject json = Dashboard::toJson();
    if (!extra.contains(QStringLiteral("order")))
        json.insert(QStringLiteral("order"), order);
    return json;
}

std::optional<Task> Task::fromJson(const QJsonObject &json, QString *error)
{
    Reader r(json);
    Task task;
    if (!r.required(QStringLiteral("id"), &task.id, error)
        || !r.required(QStringLiteral("projectId"), &task.projectId, error))
        return std::nullopt;
    r.string(QStringLiteral("title"), &task.title);
    r.boolean(QStringLiteral("isDefault"), &task.isDefault);
    r.state(QStringLiteral("state"), &task.state);
    r.priority(QStringLiteral("priority"), &task.priority);
    r.integer(QStringLiteral("order"), &task.order);
    if (!r.list(QStringLiteral("items"), &task.items, error)
        || !r.list(QStringLiteral("dashboards"), &task.dashboards, error))
        return std::nullopt;
    r.string(QStringLiteral("defaultDashboardId"), &task.defaultDashboardId);
    task.extra = r.extra();
    return task;
}

QJsonObject Task::toJson() const
{
    return withExtra({
        {QStringLiteral("id"), id},
        {QStringLiteral("projectId"), projectId},
        {QStringLiteral("title"), title},
        {QStringLiteral("isDefault"), isDefault},
        {QStringLiteral("state"), stateToString(state)},
        {QStringLiteral("priority"), static_cast<int>(priority)},
        {QStringLiteral("order"), order},
        {QStringLiteral("items"), listToJson(items)},
        {QStringLiteral("dashboards"), listToJson(dashboards)},
        {QStringLiteral("defaultDashboardId"), defaultDashboardId},
    }, extra);
}

} // namespace mirante
