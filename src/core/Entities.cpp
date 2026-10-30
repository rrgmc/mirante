#include "core/Entities.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QVariant>

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

    // An array of strings. An array with any other element stays in extra.
    void strings(const QString &key, QStringList *out)
    {
        const auto isStrings = [](const QJsonValue &v) {
            if (!v.isArray())
                return false;
            for (const QJsonValue &element : v.toArray()) {
                if (!element.isString())
                    return false;
            }
            return true;
        };
        if (const auto value = take(key, isStrings))
            *out = value->toVariant().toStringList();
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

std::optional<Group> Group::fromJson(const QJsonObject &json, QString *error)
{
    Reader r(json);
    Group group;
    if (!r.required(QStringLiteral("id"), &group.id, error))
        return std::nullopt;
    r.string(QStringLiteral("name"), &group.name);
    r.integer(QStringLiteral("order"), &group.order);
    // "webProfile" is the name used before decision 0050. "profile" wins.
    r.string(QStringLiteral("webProfile"), &group.profile);
    r.string(QStringLiteral("profile"), &group.profile);
    r.string(QStringLiteral("colour"), &group.colour);
    // "connections" and "defaultConnections" from before decision 0050 stay in
    // extra; the profile migration moves them to a profile.
    group.extra = r.extra();
    return group;
}

QJsonObject Group::toJson() const
{
    QJsonObject json{
        {QStringLiteral("id"), id},
        {QStringLiteral("name"), name},
        {QStringLiteral("order"), order},
    };
    if (!profile.isEmpty())
        json.insert(QStringLiteral("profile"), profile);
    if (!colour.isEmpty())
        json.insert(QStringLiteral("colour"), colour);
    return withExtra(json, extra);
}

bool Profile::hasData() const
{
    return !moduleSettings.isEmpty() || !extra.isEmpty();
}

std::optional<Profile> Profile::fromJson(const QJsonObject &json, QString *error)
{
    Reader r(json);
    Profile profile;
    if (!r.required(QStringLiteral("id"), &profile.id, error))
        return std::nullopt;
    r.string(QStringLiteral("name"), &profile.name);
    // "connections" and "defaultConnections" from before decision 0054 stay
    // in extra; the profile migration moves them to module settings.
    r.object(QStringLiteral("moduleSettings"), &profile.moduleSettings);
    profile.extra = r.extra();
    return profile;
}

QJsonObject Profile::toJson() const
{
    QJsonObject json{
        {QStringLiteral("id"), id},
        {QStringLiteral("name"), name},
    };
    if (!moduleSettings.isEmpty())
        json.insert(QStringLiteral("moduleSettings"), moduleSettings);
    return withExtra(json, extra);
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
    r.string(QStringLiteral("trashedAt"), &project.trashedAt);
    project.extra = r.extra();
    return project;
}

QJsonObject Project::toJson() const
{
    QJsonObject json{
        {QStringLiteral("id"), id},
        {QStringLiteral("groupId"), groupId},
        {QStringLiteral("name"), name},
        {QStringLiteral("priority"), static_cast<int>(priority)},
        {QStringLiteral("order"), order},
        {QStringLiteral("folders"), listToJson(folders)},
        {QStringLiteral("moduleSettings"), moduleSettings},
    };
    if (!trashedAt.isEmpty())
        json.insert(QStringLiteral("trashedAt"), trashedAt);
    return withExtra(json, extra);
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
    // "webProfile" is the name used before decision 0050. "profile" wins.
    r.string(QStringLiteral("webProfile"), &item.profile);
    r.string(QStringLiteral("profile"), &item.profile);
    r.boolean(QStringLiteral("watch"), &item.watch);
    r.strings(QStringLiteral("pageKeys"), &item.pageKeys);
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
    if (!profile.isEmpty())
        json.insert(QStringLiteral("profile"), profile);
    if (watch)
        json.insert(QStringLiteral("watch"), true);
    if (!pageKeys.isEmpty())
        json.insert(QStringLiteral("pageKeys"), QJsonArray::fromStringList(pageKeys));
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
    r.state(QStringLiteral("state"), &task.state);
    r.priority(QStringLiteral("priority"), &task.priority);
    r.integer(QStringLiteral("order"), &task.order);
    if (!r.list(QStringLiteral("items"), &task.items, error)
        || !r.list(QStringLiteral("dashboards"), &task.dashboards, error))
        return std::nullopt;
    r.string(QStringLiteral("defaultDashboardId"), &task.defaultDashboardId);
    r.string(QStringLiteral("trashedAt"), &task.trashedAt);
    task.extra = r.extra();
    return task;
}

QJsonObject Task::toJson() const
{
    QJsonObject json{
        {QStringLiteral("id"), id},
        {QStringLiteral("projectId"), projectId},
        {QStringLiteral("title"), title},
        {QStringLiteral("state"), stateToString(state)},
        {QStringLiteral("priority"), static_cast<int>(priority)},
        {QStringLiteral("order"), order},
        {QStringLiteral("items"), listToJson(items)},
        {QStringLiteral("dashboards"), listToJson(dashboards)},
        {QStringLiteral("defaultDashboardId"), defaultDashboardId},
    };
    if (!trashedAt.isEmpty())
        json.insert(QStringLiteral("trashedAt"), trashedAt);
    return withExtra(json, extra);
}

// toJson() writes every field and extra, so equal JSON means equal values.
bool operator==(const Folder &a, const Folder &b) { return a.toJson() == b.toJson(); }
bool operator==(const Profile &a, const Profile &b) { return a.toJson() == b.toJson(); }
bool operator==(const Group &a, const Group &b) { return a.toJson() == b.toJson(); }
bool operator==(const Project &a, const Project &b) { return a.toJson() == b.toJson(); }
bool operator==(const Item &a, const Item &b) { return a.toJson() == b.toJson(); }
bool operator==(const Dashboard &a, const Dashboard &b) { return a.toJson() == b.toJson(); }
bool operator==(const RootDashboard &a, const RootDashboard &b) { return a.toJson() == b.toJson(); }
bool operator==(const Task &a, const Task &b) { return a.toJson() == b.toJson(); }

} // namespace mirante
