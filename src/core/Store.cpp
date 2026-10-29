#include "core/Store.h"

#include "core/ConflictNames.h"
#include "core/Edits.h"
#include "core/FileWriter.h"
#include "core/FolderWatcher.h"
#include "core/Migrations.h"
#include "core/Model.h"
#include "core/ProfileMigration.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>
#include <QSaveFile>
#include <QSet>
#include <QThread>
#include <QTimer>

#include <algorithm>
#include <optional>
#include <utility>

namespace mirante {

using migrations::FileKind;

namespace {

const QString kSchemaVersion = QStringLiteral("schemaVersion");

const QString kGroups = QStringLiteral("groups");
const QString kProjects = QStringLiteral("projects");
const QString kTasks = QStringLiteral("tasks");
const QString kDashboards = QStringLiteral("dashboards");
const QString kFiles = QStringLiteral("files");
const QString kWorkspaceFile = QStringLiteral("workspace.mirante");
const QString kName = QStringLiteral("name");
const QString kProfiles = QStringLiteral("profiles");
// Keys of workspace.mirante from before decision 0050: the list of profiles,
// and module settings for the whole workspace.
const QString kOldWebProfiles = QStringLiteral("webProfiles");
const QString kOldModuleSettings = QStringLiteral("moduleSettings");
const QString kJson = QStringLiteral(".json");

// A write can fail for a moment while another program, such as a virus scanner
// or a sync tool, holds the file open. Flushing tries again a few times, and a
// write that still fails is tried again every kRetryInterval.
constexpr int kFlushAttempts = 5;
constexpr int kFlushPause = 100;      // ms
constexpr int kRetryInterval = 1000;  // ms

QString pathOf(const QString &subfolder, const QString &id)
{
    return subfolder + QLatin1Char('/') + id + kJson;
}

// What a path relative to the data folder names: an entity file or
// workspace.mirante. Anything else, such as a file in files/, names nothing.
struct FileRef
{
    FileKind kind = FileKind::Workspace;
    QString id;   // empty for workspace.mirante
};

std::optional<FileRef> refOf(const QString &relPath)
{
    if (relPath == kWorkspaceFile)
        return FileRef{FileKind::Workspace, {}};
    const int slash = relPath.indexOf(QLatin1Char('/'));
    if (slash < 0 || !relPath.endsWith(kJson))
        return std::nullopt;
    const QString subfolder = relPath.left(slash);
    const QString id = relPath.mid(slash + 1).chopped(kJson.size());
    if (id.isEmpty() || id.contains(QLatin1Char('/')))
        return std::nullopt;
    if (subfolder == kGroups)
        return FileRef{FileKind::Group, id};
    if (subfolder == kProjects)
        return FileRef{FileKind::Project, id};
    if (subfolder == kTasks)
        return FileRef{FileKind::Task, id};
    if (subfolder == kDashboards)
        return FileRef{FileKind::RootDashboard, id};
    return std::nullopt;
}

// Reads, parses and migrates the content of one file. Returns the content
// without schemaVersion, or nullopt and a message. *newer tells whether the
// file was written by a newer version of the app.
std::optional<QJsonObject> parseFile(const QByteArray &bytes, FileKind kind, QString *message, bool *newer)
{
    *newer = false;
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError) {
        *message = QStringLiteral("not valid JSON: %1").arg(error.errorString());
        return std::nullopt;
    }
    if (!document.isObject()) {
        *message = QStringLiteral("not a JSON object");
        return std::nullopt;
    }
    QJsonObject json = document.object();
    const QJsonValue version = json.value(kSchemaVersion);
    *newer = version.isDouble() && version.toDouble() > migrations::currentSchemaVersion;
    if (!migrations::migrate(kind, json, message))
        return std::nullopt;
    json.remove(kSchemaVersion);
    return json;
}

// Converts the content of the file at path with T::fromJson. The entity's id
// must match the file name.
template <typename T>
std::optional<T> entityOf(const QJsonObject &json, const QString &id, QString *message)
{
    std::optional<T> entity = T::fromJson(json, message);
    if (!entity)
        return std::nullopt;
    if (entity->id != id) {
        *message = QStringLiteral("the file name does not match the id \"%1\"").arg(entity->id);
        return std::nullopt;
    }
    return entity;
}

QString parentMissing(const QString &parentKind, const QString &parentId)
{
    return QStringLiteral("%1 \"%2\" not found").arg(parentKind, parentId);
}

// What loading a folder found, beyond the entities it put in the model.
struct Loaded
{
    std::vector<LoadProblem> problems;
    QSet<QString> newer;          // files written by a newer version of the app
    QSet<QString> orphans;        // files skipped because their parent is missing
    QJsonObject workspaceExtra;   // the keys of workspace.mirante other than name
    // What the profile migration of decision 0050 changed: these files are
    // written once the Store opens the folder.
    profilemigration::Changes migrated;
};

// One file of a subfolder: its path relative to the data folder, its id as
// given by the file name, and its content without schemaVersion.
struct EntityFile
{
    QString path;
    QString id;
    QJsonObject json;
};

// Reads every *.json file of one subfolder. A missing subfolder has no files.
std::vector<EntityFile> readFolder(const QString &dir, const QString &subfolder, FileKind kind, Loaded *loaded)
{
    std::vector<EntityFile> files;
    const QDir folder(dir + QLatin1Char('/') + subfolder);
    if (!folder.exists())
        return files;

    const QStringList names = folder.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    for (const QString &name : names) {
        // Conflict copies are listed by the Store, not loaded.
        if (!conflicts::originalOf(name).isEmpty())
            continue;
        const QString path = subfolder + QLatin1Char('/') + name;
        QFile file(folder.filePath(name));
        if (!file.open(QIODevice::ReadOnly)) {
            loaded->problems.push_back({path, QStringLiteral("cannot be read: %1").arg(file.errorString())});
            continue;
        }
        QString message;
        bool newer = false;
        std::optional<QJsonObject> json = parseFile(file.readAll(), kind, &message, &newer);
        if (!json) {
            loaded->problems.push_back({path, message});
            if (newer)
                loaded->newer.insert(path);
            continue;
        }
        files.push_back({path, name.chopped(kJson.size()), std::move(*json)});
    }
    return files;
}

template <typename T>
std::vector<T> parse(const std::vector<EntityFile> &files, Loaded *loaded)
{
    std::vector<T> entities;
    for (const EntityFile &file : files) {
        QString message;
        std::optional<T> entity = entityOf<T>(file.json, file.id, &message);
        if (!entity) {
            loaded->problems.push_back({file.path, message});
            continue;
        }
        entities.push_back(std::move(*entity));
    }
    return entities;
}

// Keeps the entities whose parent was loaded, reporting the others.
template <typename T, typename ParentId>
std::vector<T> withParent(std::vector<T> entities, const QSet<QString> &parents, ParentId parentId,
                          const QString &subfolder, const QString &parentKind, Loaded *loaded)
{
    std::vector<T> kept;
    for (T &entity : entities) {
        const QString parent = parentId(entity);
        if (parents.contains(parent)) {
            kept.push_back(std::move(entity));
        } else {
            const QString path = pathOf(subfolder, entity.id);
            loaded->problems.push_back({path, parentMissing(parentKind, parent)});
            loaded->orphans.insert(path);
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

// Reads workspace.mirante, if there is one. A missing file is not a problem.
std::optional<QJsonObject> readWorkspaceFile(const QString &dir, Loaded *loaded)
{
    QFile file(dir + QLatin1Char('/') + kWorkspaceFile);
    if (!file.exists())
        return std::nullopt;
    if (!file.open(QIODevice::ReadOnly)) {
        loaded->problems.push_back({kWorkspaceFile, QStringLiteral("cannot be read: %1").arg(file.errorString())});
        return std::nullopt;
    }
    QString message;
    bool newer = false;
    std::optional<QJsonObject> json = parseFile(file.readAll(), FileKind::Workspace, &message, &newer);
    if (!json) {
        loaded->problems.push_back({kWorkspaceFile, message});
        if (newer)
            loaded->newer.insert(kWorkspaceFile);
    }
    return json;
}

// The profiles listed in workspace.mirante, under "profiles", or under
// "webProfiles" before decision 0050. An entry that cannot be used, and one
// that repeats an id, are skipped. Default's entry holds its data; its name
// is not used. *old tells whether the old key was read.
std::vector<Profile> profilesOf(const QJsonObject &workspace, bool *old = nullptr)
{
    const bool useOld = !workspace.value(kProfiles).isArray() && workspace.value(kOldWebProfiles).isArray();
    if (old)
        *old = useOld && !workspace.value(kOldWebProfiles).toArray().isEmpty();
    std::vector<Profile> profiles;
    QSet<QString> ids;
    for (const QJsonValue &value : workspace.value(useOld ? kOldWebProfiles : kProfiles).toArray()) {
        std::optional<Profile> profile = Profile::fromJson(value.toObject());
        if (!profile || ids.contains(profile->id))
            continue;
        if (profile->id == kDefaultProfile)
            profile->name.clear();
        ids.insert(profile->id);
        profiles.push_back(std::move(*profile));
    }
    return profiles;
}

// The profiles of workspace.mirante, with the module settings from before
// decision 0050 moved into Default. *changed tells whether the old keys gave
// anything that should be written in the new form.
std::vector<Profile> migratedProfilesOf(const QJsonObject &workspace, bool *changed)
{
    bool old = false;
    std::vector<Profile> profiles = profilesOf(workspace, &old);
    // A value that is not an object is not read.
    const bool moved =
        profilemigration::moveModuleSettings(workspace.value(kOldModuleSettings).toObject(), profiles);
    *changed = old || moved;
    return profiles;
}

// The keys of workspace.mirante that the model does not hold. The keys from
// before decision 0050 are read into profiles, so they are dropped too.
QJsonObject withoutName(QJsonObject json)
{
    json.remove(kName);
    json.remove(kProfiles);
    json.remove(kOldWebProfiles);
    // A value that is not an object is not read, so it is kept as it is.
    if (json.value(kOldModuleSettings).isObject())
        json.remove(kOldModuleSettings);
    json.remove(kSchemaVersion);
    return json;
}

Loaded loadFolder(const QString &dir, Model &model)
{
    Loaded loaded;

    std::vector<Group> groups = parse<Group>(readFolder(dir, kGroups, FileKind::Group, &loaded), &loaded);
    std::vector<Project> projects = parse<Project>(readFolder(dir, kProjects, FileKind::Project, &loaded), &loaded);
    std::vector<Task> tasks = parse<Task>(readFolder(dir, kTasks, FileKind::Task, &loaded), &loaded);
    std::vector<RootDashboard> dashboards =
        parse<RootDashboard>(readFolder(dir, kDashboards, FileKind::RootDashboard, &loaded), &loaded);

    projects = withParent(std::move(projects), idsOf(groups), [](const Project &p) { return p.groupId; },
                          kProjects, QStringLiteral("group"), &loaded);
    tasks = withParent(std::move(tasks), idsOf(projects), [](const Task &t) { return t.projectId; },
                       kTasks, QStringLiteral("project"), &loaded);

    const std::optional<QJsonObject> workspace = readWorkspaceFile(dir, &loaded);

    // Decision 0050: groups' connections and the workspace's module settings
    // move into profiles. Not while workspace.mirante cannot be written over.
    std::vector<Profile> profiles;
    if (!loaded.newer.contains(kWorkspaceFile)) {
        bool workspaceMigrated = false;
        if (workspace)
            profiles = migratedProfilesOf(*workspace, &workspaceMigrated);
        loaded.migrated = profilemigration::moveConnections(groups, projects, profiles);
        loaded.migrated.profiles = loaded.migrated.profiles || workspaceMigrated;
    }

    std::sort(loaded.problems.begin(), loaded.problems.end(), [](const LoadProblem &a, const LoadProblem &b) {
        return a.file < b.file || (a.file == b.file && a.message < b.message);
    });

    model.reset(std::move(groups), std::move(projects), std::move(tasks), std::move(dashboards));
    if (workspace) {
        model.setWorkspaceName(workspace->value(kName).toString());
        loaded.workspaceExtra = withoutName(*workspace);
    }
    model.setProfiles(std::move(profiles));
    return loaded;
}

QJsonObject withSchemaVersion(QJsonObject json)
{
    json.insert(kSchemaVersion, migrations::currentSchemaVersion);
    return json;
}

} // namespace

namespace store {

std::vector<LoadProblem> load(const QString &dir, Model &model)
{
    return loadFolder(dir, model).problems;
}

std::optional<QJsonObject> fileFor(const Model &model, const QString &relPath)
{
    const std::optional<FileRef> ref = refOf(relPath);
    if (!ref)
        return std::nullopt;
    switch (ref->kind) {
    case FileKind::Workspace: {
        // Default is listed only when it holds data, and without a name.
        QJsonArray profiles;
        for (const Profile &profile : model.profiles()) {
            if (profile.id != kDefaultProfile) {
                profiles.append(profile.toJson());
            } else if (profile.hasData()) {
                QJsonObject standard = profile.toJson();
                standard.remove(kName);
                profiles.append(standard);
            }
        }
        if (model.workspaceName().isEmpty() && profiles.isEmpty())
            return std::nullopt;
        QJsonObject json{{kName, model.workspaceName()}};
        if (!profiles.isEmpty())
            json.insert(kProfiles, profiles);
        return withSchemaVersion(json);
    }
    case FileKind::Group:
        if (const Group *group = model.group(ref->id))
            return withSchemaVersion(group->toJson());
        break;
    case FileKind::Project:
        if (const Project *project = model.project(ref->id))
            return withSchemaVersion(project->toJson());
        break;
    case FileKind::Task:
        if (const Task *task = model.task(ref->id))
            return withSchemaVersion(task->toJson());
        break;
    case FileKind::RootDashboard:
        if (const RootDashboard *dashboard = model.rootDashboard(ref->id))
            return withSchemaVersion(dashboard->toJson());
        break;
    }
    return std::nullopt;
}

std::map<QString, QJsonObject> serialise(const Model &model)
{
    // Walks the hierarchy from the groups. load() never keeps an entity whose
    // parent is missing, so this reaches every project and task.
    QStringList paths;
    for (const Group *group : model.groups()) {
        paths << pathOf(kGroups, group->id);
        for (const Project *project : model.projectsOfGroup(group->id)) {
            paths << pathOf(kProjects, project->id);
            for (const Task *task : model.tasksOfProject(project->id))
                paths << pathOf(kTasks, task->id);
        }
    }
    for (const RootDashboard *dashboard : model.rootDashboards())
        paths << pathOf(kDashboards, dashboard->id);
    paths << kWorkspaceFile;

    std::map<QString, QJsonObject> files;
    for (const QString &path : std::as_const(paths)) {
        if (std::optional<QJsonObject> json = fileFor(model, path))
            files.emplace(path, std::move(*json));
    }
    return files;
}

// QJsonObject keeps its keys sorted, so the output has sorted keys.
QByteArray toBytes(const QJsonObject &json) { return QJsonDocument(json).toJson(QJsonDocument::Indented); }

bool saveWorkspaceName(const QString &dir, const QString &name, QString *error)
{
    const QString path = dir + QLatin1Char('/') + kWorkspaceFile;
    const auto fail = [error](const QString &message) {
        if (error)
            *error = message;
        return false;
    };

    // Keep the keys this version does not know.
    QJsonObject json;
    QFile existing(path);
    if (existing.exists()) {
        if (!existing.open(QIODevice::ReadOnly))
            return fail(existing.errorString());
        const QJsonDocument document = QJsonDocument::fromJson(existing.readAll());
        if (document.isObject())
            json = document.object();
        existing.close();
    }
    json.insert(kName, name);
    json.insert(kSchemaVersion, migrations::currentSchemaVersion);

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return fail(file.errorString());
    file.write(toBytes(json));
    if (!file.commit())
        return fail(file.errorString());
    return true;
}

QString workspaceFileName() { return kWorkspaceFile; }

bool isWorkspace(const QString &dir)
{
    return !dir.isEmpty() && QFileInfo(dir + QLatin1Char('/') + kWorkspaceFile).isFile();
}

bool createWorkspace(const QString &dir, const QString &name, QString *error)
{
    if (!QDir(dir).exists()) {
        if (error)
            *error = QStringLiteral("the folder does not exist");
        return false;
    }
    return saveWorkspaceName(dir, name, error);
}

} // namespace store

// The Store. Writing: each model signal names one file, which is scheduled
// with the FileWriter. Reloading: each file the FolderWatcher reports is read,
// migrated and applied with the model's setters, while `applying` makes the
// signal handlers ignore the resulting signals.
//
// Orphans: like load(), a reload skips a project or task whose parent is not
// in the model, and reports it. Such files are tried again after every reload,
// so a parent that arrives later brings them in. When an external removal of a
// group or project takes children with it (the model's removers cascade), the
// children whose files still exist are reported the same way.
struct Store::Private
{
    Store *q = nullptr;
    Model *model = nullptr;
    QString dir;
    bool readOnly = true;
    FileWriter *writer = nullptr;      // null when closed or read-only
    FolderWatcher *watcher = nullptr;  // likewise
    bool applying = false;             // a reload is being applied to the model
    QTimer retryTimer;                 // tries failed writes again

    // Problems of files on disk, from loading and reloading, by path.
    std::map<QString, QString> fileProblems;
    // Writes that failed, by path.
    std::map<QString, QString> writeProblems;
    bool problemsDirty = false;

    QSet<QString> newer;      // files of a newer schemaVersion: never written over
    QSet<QString> orphans;    // files whose parent was missing
    QJsonObject workspaceExtra;
    // The plain file names referred to by items, as of the last change.
    QSet<QString> referenced;
    QStringList conflictCopies;

    void setFileProblem(const QString &path, const QString &message);
    void clearFileProblem(const QString &path);
    void notifyProblems();

    std::vector<const Task *> allTasks() const;
    QSet<QString> referencedNames() const;
    void deleteUnreferencedFiles();

    void entitySignal(const QString &subfolder, const QString &id);
    void workspaceSignal();
    void writeMigrated(const profilemigration::Changes &changes);
    void migrateGroup(Group group);
    profilemigration::Changes pendingMigration;   // written once a reload is applied

    enum class Reload { Applied, Problem, Orphan };
    Reload reloadFile(const QString &path);
    void reloadFiles(QStringList paths);
    void removeFiles(QStringList paths);
    void retryOrphans();

    void flushWriter();
    void onConflictCopies(const QStringList &copies);
    void stop(bool notify);
};

void Store::Private::setFileProblem(const QString &path, const QString &message)
{
    auto it = fileProblems.find(path);
    if (it != fileProblems.end() && it->second == message)
        return;
    fileProblems[path] = message;
    problemsDirty = true;
}

void Store::Private::clearFileProblem(const QString &path)
{
    if (fileProblems.erase(path) > 0)
        problemsDirty = true;
}

void Store::Private::notifyProblems()
{
    if (!problemsDirty)
        return;
    problemsDirty = false;
    emit q->problemsChanged();
}

std::vector<const Task *> Store::Private::allTasks() const
{
    std::vector<const Task *> tasks;
    for (const Group *group : model->groups()) {
        for (const Project *project : model->projectsOfGroup(group->id)) {
            const std::vector<const Task *> ofProject = model->tasksOfProject(project->id);
            tasks.insert(tasks.end(), ofProject.begin(), ofProject.end());
        }
    }
    return tasks;
}

// The strings in items that could name a file in files/: a plain file name,
// without a folder.
QSet<QString> Store::Private::referencedNames() const
{
    QSet<QString> names;
    const QSet<QString> strings = edits::referencedStrings(allTasks());
    for (const QString &string : strings) {
        if (string.isEmpty() || string.startsWith(QLatin1Char('.')) || string.contains(QLatin1Char('/'))
            || string.contains(QLatin1Char('\\')))
            continue;
        names.insert(string);
    }
    return names;
}

// After a change made on this device: deletes the files in files/ that were
// referred to before it and are not any more. A file found unreferenced on disk
// is never deleted. See docs/decisions/0030-deleting-image-files.md.
void Store::Private::deleteUnreferencedFiles()
{
    const QSet<QString> now = referencedNames();
    for (const QString &name : std::as_const(referenced)) {
        if (now.contains(name))
            continue;
        const QString relPath = kFiles + QLatin1Char('/') + name;
        if (QFileInfo(dir + QLatin1Char('/') + relPath).isFile())
            writer->schedule(relPath, std::nullopt);
    }
    referenced = now;
}

void Store::Private::entitySignal(const QString &subfolder, const QString &id)
{
    if (!writer || applying)
        return;
    const QString path = pathOf(subfolder, id);
    // A file this app cannot read is not written over.
    if (!newer.contains(path)) {
        const std::optional<QJsonObject> json = store::fileFor(*model, path);
        writer->schedule(path, json ? std::optional<QByteArray>(store::toBytes(*json)) : std::nullopt);
    }
    if (subfolder == kTasks)
        deleteUnreferencedFiles();
}

void Store::Private::workspaceSignal()
{
    if (!writer || applying || newer.contains(kWorkspaceFile))
        return;
    // workspace.mirante is never deleted: the folder would stop being a
    // workspace. An empty name is written as such.
    const QJsonObject file = store::fileFor(*model, kWorkspaceFile)
                                 .value_or(withSchemaVersion(QJsonObject{{kName, QString()}}));
    QJsonObject json = workspaceExtra;
    for (auto it = file.begin(); it != file.end(); ++it)
        json.insert(it.key(), it.value());
    writer->schedule(kWorkspaceFile, store::toBytes(json));
}

// Writes the files a profile migration changed: workspace.mirante first, at
// once, so the connections are stored in a profile before the groups lose
// them. If it cannot be written yet, the groups keep their old keys on disk,
// and the next load moves them again without repeating anything.
void Store::Private::writeMigrated(const profilemigration::Changes &changes)
{
    if (!writer || !changes.any() || newer.contains(kWorkspaceFile))
        return;
    if (changes.profiles) {
        workspaceSignal();
        flushWriter();
        if (writer->pending())
            return;
    }
    for (const QString &id : changes.groups)
        entitySignal(kGroups, id);
    for (const QString &id : changes.projects)
        entitySignal(kProjects, id);
}

// A reloaded group that still has connections from before decision 0050, for
// example from a device with an older version: they move to its profile, as
// on load. Called while applying; reloadFiles() writes the changes after.
void Store::Private::migrateGroup(Group group)
{
    std::vector<Group> groups{std::move(group)};
    std::vector<Project> projects;
    for (const Project *project : model->projectsOfGroup(groups.front().id))
        projects.push_back(*project);
    std::vector<Profile> profiles = model->profiles();
    const profilemigration::Changes changes = profilemigration::moveConnections(groups, projects, profiles);
    model->setProfiles(std::move(profiles));
    model->setGroup(groups.front());
    for (const Project &project : projects) {
        if (changes.projects.contains(project.id))
            model->setProject(project);
    }
    pendingMigration.profiles = pendingMigration.profiles || changes.profiles;
    pendingMigration.groups << changes.groups;
    pendingMigration.projects << changes.projects;
}

Store::Private::Reload Store::Private::reloadFile(const QString &path)
{
    const std::optional<FileRef> ref = refOf(path);
    if (!ref)
        return Reload::Applied;   // files/ and the like hold nothing to load

    QFile file(dir + QLatin1Char('/') + path);
    if (!file.exists())
        return Reload::Applied;   // its removal is reported apart
    if (!file.open(QIODevice::ReadOnly)) {
        setFileProblem(path, QStringLiteral("cannot be read: %1").arg(file.errorString()));
        return Reload::Problem;
    }

    // A file that cannot be used is reported, and the model keeps the entity
    // as it was.
    QString message;
    bool isNewer = false;
    const std::optional<QJsonObject> json = parseFile(file.readAll(), ref->kind, &message, &isNewer);
    if (isNewer)
        newer.insert(path);
    else
        newer.remove(path);
    const auto problem = [&] {
        setFileProblem(path, message);
        return Reload::Problem;
    };
    if (!json)
        return problem();

    applying = true;
    Reload result = Reload::Applied;
    switch (ref->kind) {
    case FileKind::Workspace:
        workspaceExtra = withoutName(*json);
        model->setWorkspaceName(json->value(kName).toString());
        // Old keys are migrated in the model only: a reload is never written
        // back. The next write of the file drops them.
        {
            bool changed = false;
            model->setProfiles(migratedProfilesOf(*json, &changed));
        }
        break;
    case FileKind::Group:
        if (std::optional<Group> group = entityOf<Group>(*json, ref->id, &message)) {
            if (profilemigration::hasOldConnections(*group) && !newer.contains(kWorkspaceFile))
                migrateGroup(std::move(*group));
            else
                model->setGroup(*group);
        } else
            result = Reload::Problem;
        break;
    case FileKind::Project:
        if (const std::optional<Project> project = entityOf<Project>(*json, ref->id, &message)) {
            if (model->group(project->groupId))
                model->setProject(*project);
            else {
                message = parentMissing(QStringLiteral("group"), project->groupId);
                result = Reload::Orphan;
            }
        } else {
            result = Reload::Problem;
        }
        break;
    case FileKind::Task:
        if (const std::optional<Task> task = entityOf<Task>(*json, ref->id, &message)) {
            if (model->project(task->projectId))
                model->setTask(*task);
            else {
                message = parentMissing(QStringLiteral("project"), task->projectId);
                result = Reload::Orphan;
            }
        } else {
            result = Reload::Problem;
        }
        break;
    case FileKind::RootDashboard:
        if (const std::optional<RootDashboard> dashboard = entityOf<RootDashboard>(*json, ref->id, &message))
            model->setRootDashboard(*dashboard);
        else
            result = Reload::Problem;
        break;
    }
    applying = false;

    if (result == Reload::Orphan)
        orphans.insert(path);
    else
        orphans.remove(path);
    if (result == Reload::Applied)
        clearFileProblem(path);
    else
        setFileProblem(path, message);
    return result;
}

namespace {

// The order in which files are applied, parents before children, and root
// dashboards after the tasks their cells show.
int reloadRank(const QString &path)
{
    const std::optional<FileRef> ref = refOf(path);
    if (!ref)
        return 5;
    switch (ref->kind) {
    case FileKind::Workspace: return 0;
    case FileKind::Group: return 1;
    case FileKind::Project: return 2;
    case FileKind::Task: return 3;
    case FileKind::RootDashboard: return 4;
    }
    return 5;
}

} // namespace

namespace {

bool hasRemovedCell(const LayoutNode &node)
{
    if (node.removed)
        return true;
    return std::any_of(node.children.begin(), node.children.end(), hasRemovedCell);
}

} // namespace

void Store::Private::reloadFiles(QStringList paths)
{
    std::stable_sort(paths.begin(), paths.end(),
                     [](const QString &a, const QString &b) { return reloadRank(a) < reloadRank(b); });
    for (const QString &path : std::as_const(paths))
        reloadFile(path);
    retryOrphans();

    // A root dashboard that synced before the tasks it shows lost those cells
    // to normalisation. Its file still has them, so read it again: the cells
    // come back once their tasks exist. Such dashboards are rare and small.
    QStringList again;
    for (const RootDashboard *dashboard : model->rootDashboards()) {
        const QString path = pathOf(kDashboards, dashboard->id);
        if (!paths.contains(path) && hasRemovedCell(dashboard->layout))
            again << path;
    }
    for (const QString &path : std::as_const(again))
        reloadFile(path);
    // A reload never deletes files; it only moves the baseline.
    referenced = referencedNames();
    // Groups from before decision 0050 are written once, migrated.
    writeMigrated(std::exchange(pendingMigration, {}));
    notifyProblems();
}

void Store::Private::retryOrphans()
{
    bool progress = true;
    while (progress && !orphans.isEmpty()) {
        progress = false;
        const QSet<QString> paths = orphans;
        for (const QString &path : paths) {
            if (!QFileInfo::exists(dir + QLatin1Char('/') + path)) {
                orphans.remove(path);
                continue;
            }
            if (reloadFile(path) == Reload::Applied)
                progress = true;
        }
    }
}

void Store::Private::removeFiles(QStringList paths)
{
    // Children first, so a cascade in the model takes only entities whose
    // files are still there.
    std::stable_sort(paths.begin(), paths.end(),
                     [](const QString &a, const QString &b) { return reloadRank(a) > reloadRank(b); });

    // The children a cascade removes, with the problem to report if their file
    // is still on disk.
    std::vector<std::pair<QString, QString>> cascaded;
    const auto collectTasks = [&](const QString &projectId) {
        for (const Task *task : model->tasksOfProject(projectId))
            cascaded.emplace_back(pathOf(kTasks, task->id), parentMissing(QStringLiteral("project"), projectId));
    };

    applying = true;
    for (const QString &path : std::as_const(paths)) {
        clearFileProblem(path);
        orphans.remove(path);
        newer.remove(path);
        const std::optional<FileRef> ref = refOf(path);
        if (!ref)
            continue;
        switch (ref->kind) {
        case FileKind::Workspace:
            setFileProblem(path, QStringLiteral("was removed, so this folder is no longer a workspace"));
            break;
        case FileKind::Group:
            for (const Project *project : model->projectsOfGroup(ref->id)) {
                cascaded.emplace_back(pathOf(kProjects, project->id), parentMissing(QStringLiteral("group"), ref->id));
                collectTasks(project->id);
            }
            model->removeGroup(ref->id);
            break;
        case FileKind::Project:
            collectTasks(ref->id);
            model->removeProject(ref->id);
            break;
        case FileKind::Task:
            model->removeTask(ref->id);
            break;
        case FileKind::RootDashboard:
            model->removeRootDashboard(ref->id);
            break;
        }
    }
    applying = false;

    for (const auto &[path, message] : cascaded) {
        if (QFileInfo::exists(dir + QLatin1Char('/') + path)) {
            setFileProblem(path, message);
            orphans.insert(path);
        }
    }
    referenced = referencedNames();
    notifyProblems();
}

void Store::Private::onConflictCopies(const QStringList &copies)
{
    if (copies == conflictCopies)
        return;
    conflictCopies = copies;
    emit q->conflictsChanged();
}

void Store::Private::flushWriter()
{
    if (!writer)
        return;
    writer->flush();
    for (int attempt = 1; attempt < kFlushAttempts && writer->pending(); ++attempt) {
        QThread::msleep(kFlushPause);
        writer->flush();
    }
    if (writer->pending())
        retryTimer.start();
}

void Store::Private::stop(bool notify)
{
    retryTimer.stop();
    if (writer) {
        // Without notify the Store is being destroyed: nothing is reported.
        if (!notify)
            QObject::disconnect(writer, nullptr, q, nullptr);
        flushWriter();
        retryTimer.stop();
    }
    delete watcher;
    watcher = nullptr;
    delete writer;
    writer = nullptr;

    dir.clear();
    readOnly = true;
    newer.clear();
    orphans.clear();
    workspaceExtra = {};
    referenced.clear();
    const bool hadProblems = !fileProblems.empty() || !writeProblems.empty();
    fileProblems.clear();
    writeProblems.clear();
    problemsDirty = false;
    const bool hadConflicts = !conflictCopies.isEmpty();
    conflictCopies.clear();
    if (notify && hadProblems)
        emit q->problemsChanged();
    if (notify && hadConflicts)
        emit q->conflictsChanged();
}

Store::Store(Model &model, QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
    d->q = this;
    d->model = &model;

    d->retryTimer.setSingleShot(true);
    d->retryTimer.setInterval(kRetryInterval);
    connect(&d->retryTimer, &QTimer::timeout, this, [this] {
        if (!d->writer)
            return;
        d->writer->flush();
        if (d->writer->pending())
            d->retryTimer.start();
    });

    const auto on = [this](const QString &subfolder) {
        return [this, subfolder](const QString &id) { d->entitySignal(subfolder, id); };
    };
    connect(&model, &Model::groupAdded, this, on(kGroups));
    connect(&model, &Model::groupChanged, this, on(kGroups));
    connect(&model, &Model::groupRemoved, this, on(kGroups));
    connect(&model, &Model::projectAdded, this, on(kProjects));
    connect(&model, &Model::projectChanged, this, on(kProjects));
    connect(&model, &Model::projectRemoved, this, on(kProjects));
    connect(&model, &Model::taskAdded, this, on(kTasks));
    connect(&model, &Model::taskChanged, this, on(kTasks));
    connect(&model, &Model::taskRemoved, this, on(kTasks));
    connect(&model, &Model::rootDashboardAdded, this, on(kDashboards));
    connect(&model, &Model::rootDashboardChanged, this, on(kDashboards));
    connect(&model, &Model::rootDashboardRemoved, this, on(kDashboards));
    connect(&model, &Model::workspaceNameChanged, this, [this] { d->workspaceSignal(); });
    connect(&model, &Model::profilesChanged, this, [this] { d->workspaceSignal(); });
    // modelReset comes from loading, which writes nothing.
}

Store::~Store()
{
    d->stop(false);
}

std::vector<LoadProblem> Store::open(const QString &dir)
{
    d->stop(true);

    // The writer does not exist yet, so loading writes nothing.
    Loaded loaded = loadFolder(dir, *d->model);
    d->dir = dir;
    d->readOnly = dir.isEmpty() || dir.startsWith(QLatin1Char(':'));
    for (const LoadProblem &problem : loaded.problems)
        d->fileProblems[problem.file] = problem.message;
    d->newer = std::move(loaded.newer);
    d->orphans = std::move(loaded.orphans);
    d->workspaceExtra = std::move(loaded.workspaceExtra);
    d->referenced = d->referencedNames();

    if (!d->readOnly) {
        d->writer = new FileWriter(dir, this);
        FileWriter *writer = d->writer;
        d->watcher = new FolderWatcher(
            dir, [writer](const QString &relPath, const QByteArray &content) {
                return writer->isOwnWrite(relPath, content);
            },
            this);

        connect(writer, &FileWriter::writeFailed, this, [this](const QString &relPath, const QString &message) {
            d->writeProblems[relPath] = QStringLiteral("could not be saved: %1").arg(message);
            d->problemsDirty = true;
            d->retryTimer.start();
            d->notifyProblems();
        });
        connect(writer, &FileWriter::written, this, [this](const QString &relPath) {
            // What this app wrote replaces whatever was wrong with the file.
            if (d->writeProblems.erase(relPath) > 0)
                d->problemsDirty = true;
            d->clearFileProblem(relPath);
            d->orphans.remove(relPath);
            d->notifyProblems();
        });
        connect(d->watcher, &FolderWatcher::filesChanged, this,
                [this](const QStringList &relPaths) { d->reloadFiles(relPaths); });
        connect(d->watcher, &FolderWatcher::filesRemoved, this,
                [this](const QStringList &relPaths) { d->removeFiles(relPaths); });
        connect(d->watcher, &FolderWatcher::conflictCopiesChanged, this,
                [this](const QStringList &relPaths) { d->onConflictCopies(relPaths); });
        d->watcher->start();
        // The profile migration of decision 0050 is written once.
        d->writeMigrated(loaded.migrated);
    }

    d->problemsDirty = false;
    emit problemsChanged();
    return loaded.problems;
}

void Store::close()
{
    d->stop(true);
}

QString Store::dir() const { return d->dir; }

bool Store::readOnly() const { return d->readOnly; }

void Store::flush()
{
    d->flushWriter();
}

std::vector<LoadProblem> Store::problems() const
{
    std::vector<LoadProblem> problems;
    for (const auto &[file, message] : d->fileProblems)
        problems.push_back({file, message});
    for (const auto &[file, message] : d->writeProblems)
        problems.push_back({file, message});
    std::sort(problems.begin(), problems.end(), [](const LoadProblem &a, const LoadProblem &b) {
        return a.file < b.file || (a.file == b.file && a.message < b.message);
    });
    return problems;
}

QString Store::saveFile(const QByteArray &data, const QString &extension, QString *error)
{
    const auto fail = [error](const QString &message) {
        if (error)
            *error = message;
        return QString();
    };
    if (!d->writer)
        return fail(QStringLiteral("the workspace is read-only"));

    QString suffix = extension;
    while (suffix.startsWith(QLatin1Char('.')))
        suffix.remove(0, 1);
    const QString name = suffix.isEmpty() ? edits::newId() : edits::newId() + QLatin1Char('.') + suffix;

    // Written at once, so the caller can show the file. The watcher reports it,
    // and a reload ignores files/.
    const QString folder = d->dir + QLatin1Char('/') + kFiles;
    if (!QDir().mkpath(folder))
        return fail(QStringLiteral("could not create the folder %1").arg(QDir::toNativeSeparators(folder)));
    QSaveFile file(folder + QLatin1Char('/') + name);
    if (!file.open(QIODevice::WriteOnly))
        return fail(file.errorString());
    if (file.write(data) != data.size()) {
        const QString message = file.errorString();
        file.cancelWriting();
        return fail(message);
    }
    if (!file.commit())
        return fail(file.errorString());
    return name;
}

QString Store::filePath(const QString &name) const
{
    return d->dir + QLatin1Char('/') + kFiles + QLatin1Char('/') + name;
}

namespace {

// The file a conflict copy is a copy of, both relative to the data folder.
QString originalPath(const QString &copy)
{
    const int slash = copy.lastIndexOf(QLatin1Char('/'));
    const QString original = conflicts::originalOf(copy.mid(slash + 1));
    if (original.isEmpty())
        return {};
    return slash < 0 ? original : copy.left(slash + 1) + original;
}

} // namespace

std::vector<Store::Conflict> Store::conflicts() const
{
    std::vector<Conflict> result;
    for (const QString &copy : std::as_const(d->conflictCopies)) {
        const QString file = originalPath(copy);
        if (!file.isEmpty())
            result.push_back({file, copy});
    }
    return result;
}

void Store::keepCurrent(const Conflict &conflict)
{
    if (!d->writer || !d->conflictCopies.contains(conflict.copy))
        return;
    d->writer->schedule(conflict.copy, std::nullopt);
    d->flushWriter();
    if (QFileInfo::exists(d->dir + QLatin1Char('/') + conflict.copy))
        return;   // reported through writeFailed
    QStringList copies = d->conflictCopies;
    copies.removeAll(conflict.copy);
    d->onConflictCopies(copies);
}

void Store::useCopy(const Conflict &conflict)
{
    if (!d->writer || !d->conflictCopies.contains(conflict.copy) || originalPath(conflict.copy) != conflict.file)
        return;

    QFile copy(d->dir + QLatin1Char('/') + conflict.copy);
    if (!copy.open(QIODevice::ReadOnly)) {
        d->writeProblems[conflict.copy] = QStringLiteral("cannot be read: %1").arg(copy.errorString());
        d->problemsDirty = true;
        d->notifyProblems();
        return;
    }
    const QByteArray content = copy.readAll();
    copy.close();

    // Pending local writes go first, so none of them lands on the copy's content.
    d->flushWriter();
    d->writer->schedule(conflict.file, content);
    d->flushWriter();
    QFile written(d->dir + QLatin1Char('/') + conflict.file);
    if (!written.open(QIODevice::ReadOnly) || written.readAll() != content)
        return;   // reported through writeFailed
    written.close();
    d->writer->schedule(conflict.copy, std::nullopt);
    d->flushWriter();

    // The writer recorded the content as its own, so the watcher will not
    // report it: apply it here.
    d->reloadFiles({conflict.file});

    QStringList copies = d->conflictCopies;
    copies.removeAll(conflict.copy);
    d->onConflictCopies(copies);
}

} // namespace mirante
