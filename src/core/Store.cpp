#include "core/Store.h"

#include "core/Model.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>
#include <QSet>

#include <algorithm>
#include <optional>
#include <utility>

namespace mirante::store {

namespace {

const QString kSchemaVersion = QStringLiteral("schemaVersion");
constexpr int kCurrentSchemaVersion = 1;

const QString kGroups = QStringLiteral("groups");
const QString kProjects = QStringLiteral("projects");
const QString kTasks = QStringLiteral("tasks");
const QString kDashboards = QStringLiteral("dashboards");

QString pathOf(const QString &subfolder, const QString &id)
{
    return subfolder + QLatin1Char('/') + id + QStringLiteral(".json");
}

// One file of a subfolder: its path relative to the data folder, its id as
// given by the file name, and its content without schemaVersion.
struct EntityFile
{
    QString path;
    QString id;
    QJsonObject json;
};

// Reads every *.json file of one subfolder. A missing subfolder has no files.
std::vector<EntityFile> readFolder(const QString &dir, const QString &subfolder, std::vector<LoadProblem> *problems)
{
    std::vector<EntityFile> files;
    const QDir folder(dir + QLatin1Char('/') + subfolder);
    if (!folder.exists())
        return files;

    const QStringList names = folder.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    for (const QString &name : names) {
        const QString path = subfolder + QLatin1Char('/') + name;
        const auto problem = [&](const QString &message) { problems->push_back({path, message}); };

        QFile file(folder.filePath(name));
        if (!file.open(QIODevice::ReadOnly)) {
            problem(QStringLiteral("cannot be read: %1").arg(file.errorString()));
            continue;
        }
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
        if (error.error != QJsonParseError::NoError) {
            problem(QStringLiteral("not valid JSON: %1").arg(error.errorString()));
            continue;
        }
        if (!document.isObject()) {
            problem(QStringLiteral("not a JSON object"));
            continue;
        }
        QJsonObject json = document.object();
        const QJsonValue version = json.value(kSchemaVersion);
        if (!version.isDouble() || version.toDouble() != kCurrentSchemaVersion) {
            problem(QStringLiteral("unsupported schemaVersion"));
            continue;
        }
        json.remove(kSchemaVersion);
        files.push_back({path, name.chopped(5), std::move(json)});
    }
    return files;
}

// Converts the files of one subfolder with T::fromJson. Each entity's id must
// match its file name.
template <typename T>
std::vector<T> parse(const std::vector<EntityFile> &files, std::vector<LoadProblem> *problems)
{
    std::vector<T> entities;
    for (const EntityFile &file : files) {
        QString error;
        std::optional<T> entity = T::fromJson(file.json, &error);
        if (!entity) {
            problems->push_back({file.path, error});
            continue;
        }
        if (entity->id != file.id) {
            problems->push_back({file.path, QStringLiteral("the file name does not match the id \"%1\"").arg(entity->id)});
            continue;
        }
        entities.push_back(std::move(*entity));
    }
    return entities;
}

// Keeps the entities whose parent was loaded, reporting the others.
template <typename T, typename ParentId>
std::vector<T> withParent(std::vector<T> entities, const QSet<QString> &parents, ParentId parentId,
                          const QString &subfolder, const QString &parentKind, std::vector<LoadProblem> *problems)
{
    std::vector<T> kept;
    for (T &entity : entities) {
        const QString parent = parentId(entity);
        if (parents.contains(parent)) {
            kept.push_back(std::move(entity));
        } else {
            problems->push_back({pathOf(subfolder, entity.id),
                                 QStringLiteral("%1 \"%2\" not found").arg(parentKind, parent)});
        }
    }
    return kept;
}

template <typename T>
QSet<QString> idsOf(const std::vector<T> &entities)
{
    QSet<QString> ids;
    for (const T &entity : entities)
        ids.insert(entity.id);
    return ids;
}

QJsonObject withSchemaVersion(QJsonObject json)
{
    json.insert(kSchemaVersion, kCurrentSchemaVersion);
    return json;
}

} // namespace

std::vector<LoadProblem> load(const QString &dir, Model &model)
{
    std::vector<LoadProblem> problems;

    std::vector<Group> groups = parse<Group>(readFolder(dir, kGroups, &problems), &problems);
    std::vector<Project> projects = parse<Project>(readFolder(dir, kProjects, &problems), &problems);
    std::vector<Task> tasks = parse<Task>(readFolder(dir, kTasks, &problems), &problems);
    std::vector<RootDashboard> dashboards = parse<RootDashboard>(readFolder(dir, kDashboards, &problems), &problems);

    projects = withParent(std::move(projects), idsOf(groups), [](const Project &p) { return p.groupId; },
                          kProjects, QStringLiteral("group"), &problems);
    tasks = withParent(std::move(tasks), idsOf(projects), [](const Task &t) { return t.projectId; },
                       kTasks, QStringLiteral("project"), &problems);

    std::sort(problems.begin(), problems.end(), [](const LoadProblem &a, const LoadProblem &b) {
        return a.file < b.file || (a.file == b.file && a.message < b.message);
    });

    model.reset(std::move(groups), std::move(projects), std::move(tasks), std::move(dashboards));
    return problems;
}

std::map<QString, QJsonObject> serialise(const Model &model)
{
    // Walks the hierarchy from the groups. load() never keeps an entity whose
    // parent is missing, so this reaches every project and task.
    std::map<QString, QJsonObject> files;
    for (const Group *group : model.groups()) {
        files.emplace(pathOf(kGroups, group->id), withSchemaVersion(group->toJson()));
        for (const Project *project : model.projectsOfGroup(group->id)) {
            files.emplace(pathOf(kProjects, project->id), withSchemaVersion(project->toJson()));
            for (const Task *task : model.tasksOfProject(project->id))
                files.emplace(pathOf(kTasks, task->id), withSchemaVersion(task->toJson()));
        }
    }
    for (const RootDashboard *dashboard : model.rootDashboards())
        files.emplace(pathOf(kDashboards, dashboard->id), withSchemaVersion(dashboard->toJson()));
    return files;
}

} // namespace mirante::store
