#include "api/StatusCache.h"

#include "api/ModuleDirectory.h"
#include "api/ServerClient.h"
#include "core/DeviceSettings.h"
#include "core/GitHead.h"
#include "core/Model.h"
#include "core/Profiles.h"

#include <mirante/ApiClient.h>
#include <mirante/ItemType.h>

#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QPointer>
#include <QTimer>

#include <algorithm>
#include <map>
#include <set>
#include <vector>

namespace mirante {

namespace {

constexpr int tickMs = 15 * 1000;
constexpr qint64 jobTimeoutSecs = 90;
constexpr qint64 openedTaskSecs = 30 * 60;

// The module of a type id: the part before the first dot, such as "gitlab"
// for "gitlab.mr.summary".
QString moduleOf(const QString &typeId)
{
    return typeId.section(QLatin1Char('.'), 0, 0);
}

// The server of an item whose module has none, or whose client could not be
// made: every request ends with NoServer, without being sent. Answers come
// on a later pass of the event loop, as from a real server.
class NoServerClient : public ApiClient
{
public:
    QUrl baseUrl() const override { return {}; }

    void get(const QString &, const QUrlQuery &, const GetOptions &, QObject *receiver,
             std::function<void(const ApiReply &)> done) override
    {
        answer(receiver, std::move(done));
    }

    void post(const QString &, const QJsonDocument &, QObject *receiver,
              std::function<void(const ApiReply &)> done) override
    {
        answer(receiver, std::move(done));
    }

    void put(const QString &, const QJsonDocument &, QObject *receiver,
             std::function<void(const ApiReply &)> done) override
    {
        answer(receiver, std::move(done));
    }

    bool authorize(QNetworkRequest &) override { return false; }

    void secret(QObject *receiver, std::function<void(std::optional<QString>)> done) override
    {
        if (!receiver || !done)
            return;
        QTimer::singleShot(0, receiver, [done = std::move(done)] { done(std::nullopt); });
    }

private:
    static void answer(QObject *receiver, std::function<void(const ApiReply &)> done)
    {
        if (!receiver || !done)
            return;
        QTimer::singleShot(0, receiver, [done = std::move(done)] {
            ApiReply reply;
            reply.error = ApiReply::Error::NoServer;
            reply.message = StatusCache::tr("No server is set for this module.");
            done(reply);
        });
    }
};

class StatusJobImpl;

// One item's entry. It is also the item's ItemStatusSource, so it lives as
// long as the cache: removing the item only empties it.
class StatusEntry : public ItemStatusSource
{
public:
    StatusEntry(StatusCache &cache, const QString &itemId)
        : cache(cache)
        , itemId(itemId)
    {
    }

    ItemStatus current() const override { return status; }
    void refresh() override { cache.refresh(itemId); }

    void subscribe(QObject *receiver, std::function<void(const ItemStatus &)> changed) override
    {
        if (receiver && changed)
            subscribers.push_back({receiver, std::move(changed)});
    }

    void notify()
    {
        // A subscriber may subscribe or be destroyed while being called, so
        // the list is copied first.
        const auto copy = subscribers;
        for (const auto &subscriber : copy) {
            if (subscriber.receiver)
                subscriber.changed(status);
        }
        subscribers.erase(std::remove_if(subscribers.begin(), subscribers.end(),
                                         [](const Subscriber &s) { return s.receiver.isNull(); }),
                          subscribers.end());
    }

    struct Subscriber
    {
        QPointer<QObject> receiver;
        std::function<void(const ItemStatus &)> changed;
    };

    StatusCache &cache;
    const QString itemId;
    ItemStatus status;
    QDateTime fetchedAt;            // when the last fetch ended; invalid to fetch again soon
    int visible = 0;                // placements showing the item
    std::vector<Subscriber> subscribers;
    StatusJobImpl *job = nullptr;
    QDateTime jobStarted;
    std::set<QString> heads;        // the Git head files the last finished job read
};

// One fetch. It is the job's owner too, so requests made with owner() as the
// receiver end without a callback once the job is gone.
class StatusJobImpl : public QObject, public StatusJob
{
public:
    using Done = std::function<void(StatusJobImpl *, const ItemStatus &)>;

    // folders holds the local path of each of the project's folders, by id,
    // and of the main folder under an empty id.
    StatusJobImpl(QJsonObject config, QJsonObject projectSettings, QString profileId, QString moduleId,
                  std::map<QString, QString> folders, const Model &model, ApiClient &server, Done done)
        : m_config(std::move(config))
        , m_projectSettings(std::move(projectSettings))
        , m_profileId(std::move(profileId))
        , m_moduleId(std::move(moduleId))
        , m_folders(std::move(folders))
        , m_model(model)
        , m_server(server)
        , m_done(std::move(done))
    {
    }

    QJsonObject config() const override { return m_config; }
    QJsonValue projectSetting(const QString &key) const override { return m_projectSettings.value(key); }
    QJsonObject moduleSettings(const QString &screenId) const override
    {
        return m_model.moduleSettings(m_profileId, m_moduleId, screenId);
    }
    ApiClient &server() override { return m_server; }
    QObject *owner() override { return this; }

    QString folderPath(const QString &folderId) const override
    {
        const auto it = m_folders.find(folderId);
        return it == m_folders.end() ? QString() : it->second;
    }

    QString gitHead(const QString &path) override
    {
        if (path.isEmpty())
            return {};
        const QString file = git::headFile(path);
        if (!file.isEmpty())
            m_heads.insert(file);
        return git::branchAt(path);
    }

    // The head files gitHead() read, for the cache to watch.
    const std::set<QString> &heads() const { return m_heads; }

    void finish(const ItemStatus &status) override
    {
        if (m_finished)
            return;
        m_finished = true;
        // The type may still be inside fetchStatus(), so the job goes later.
        deleteLater();
        if (m_done)
            m_done(this, status);
    }

    // Ends the job without a status: the cache gave up on it or dropped it.
    void abandon()
    {
        m_finished = true;
        m_done = nullptr;
        deleteLater();
    }

private:
    QJsonObject m_config;
    QJsonObject m_projectSettings;
    QString m_profileId;
    QString m_moduleId;
    std::map<QString, QString> m_folders;
    std::set<QString> m_heads;
    const Model &m_model;
    ApiClient &m_server;
    Done m_done;
    bool m_finished = false;
};

} // namespace

struct StatusCache::Private
{
    StatusCache *q = nullptr;
    Model *model = nullptr;
    const ModuleDirectory *modules = nullptr;
    ServerClients *servers = nullptr;
    DeviceSettings *settings = nullptr;
    Clock clock;
    QTimer timer;
    NoServerClient noServer;
    // Watches the Git head files that jobs read. See decision 0080.
    QFileSystemWatcher headWatcher;

    std::map<QString, std::unique_ptr<StatusEntry>> entries;
    // When each task was last opened.
    std::map<QString, QDateTime> openedTasks;

    // What the cache last saw of each item in the model: its task, and what a
    // fetch depends on. A change to the latter fetches again.
    struct Known
    {
        QString taskId;
        QJsonObject inputs;
    };
    std::map<QString, Known> known;

    QDateTime now() const { return clock ? clock() : QDateTime::currentDateTimeUtc(); }

    StatusEntry &entry(const QString &itemId)
    {
        auto &slot = entries[itemId];
        if (!slot)
            slot = std::make_unique<StatusEntry>(*q, itemId);
        return *slot;
    }

    StatusEntry *findEntry(const QString &itemId) const
    {
        const auto it = entries.find(itemId);
        return it == entries.end() ? nullptr : it->second.get();
    }

    template<typename F>
    void forEachTask(F f) const
    {
        for (const Group *group : model->groups()) {
            for (const Project *project : model->projectsOfGroup(group->id)) {
                for (const Task *task : model->tasksOfProject(project->id))
                    f(*task);
            }
        }
    }

    // The local paths of a project's folders on this device, by folder id,
    // and the main folder's under an empty id.
    std::map<QString, QString> foldersOf(const QString &projectId) const
    {
        std::map<QString, QString> paths;
        const Project *project = model->project(projectId);
        if (!project)
            return paths;
        for (const Folder &folder : project->folders) {
            const QString path = settings->folderPath(folder.id);
            paths[folder.id] = path;
            if (folder.main)
                paths[QString()] = path;
        }
        return paths;
    }

    static QJsonObject toJson(const std::map<QString, QString> &paths)
    {
        QJsonObject json;
        for (const auto &[id, path] : paths)
            json.insert(id, path);
        return json;
    }

    // What a fetch of the item depends on: its type and configuration, its
    // profile, its module's settings in the profile and the project, and the
    // local paths of the project's folders.
    QJsonObject inputsOf(const Task &task, const Item &item, const QJsonObject &folders) const
    {
        const QString moduleId = moduleOf(item.type);
        const QString profileId = profiles::effective(*model, task.id, item.id);
        QJsonObject inputs;
        inputs.insert(QStringLiteral("type"), item.type);
        inputs.insert(QStringLiteral("config"), item.config);
        inputs.insert(QStringLiteral("profile"), profileId);
        if (const Profile *profile = model->profile(profileId))
            inputs.insert(QStringLiteral("profileSettings"), profile->moduleSettings.value(moduleId));
        if (const Project *project = model->project(task.projectId))
            inputs.insert(QStringLiteral("projectSettings"), project->moduleSettings.value(moduleId));
        inputs.insert(QStringLiteral("folders"), folders);
        return inputs;
    }

    // Brings `known` up to date with the model. Items gone are emptied, and
    // items whose inputs changed are fetched again. forgetSeen also removes
    // the seen attention keys of the items gone; a workspace being loaded
    // keeps them, since they belong to the workspace that was open.
    void sync(bool forgetSeen)
    {
        std::map<QString, Known> now;
        std::map<QString, QJsonObject> folders;   // by project id
        forEachTask([&](const Task &task) {
            auto it = folders.find(task.projectId);
            if (it == folders.end())
                it = folders.emplace(task.projectId, toJson(foldersOf(task.projectId))).first;
            for (const Item &item : task.items)
                now[item.id] = Known{task.id, inputsOf(task, item, it->second)};
        });

        std::vector<QString> removed;
        std::vector<QString> changed;
        for (const auto &[itemId, before] : known) {
            const auto it = now.find(itemId);
            if (it == now.end())
                removed.push_back(itemId);
            else if (it->second.inputs != before.inputs)
                changed.push_back(itemId);
        }
        known = std::move(now);

        for (const QString &itemId : removed) {
            if (forgetSeen) {
                settings->removeSeenAttention(model->workspaceId(), itemId);
                settings->removeNotifiedAttention(model->workspaceId(), itemId);
            }
            empty(itemId);
        }
        for (const QString &itemId : changed)
            fetchAgain(itemId);
    }

    // Fetches an item whose inputs changed at once, if it is due.
    void fetchAgain(const QString &itemId)
    {
        if (StatusEntry *e = findEntry(itemId)) {
            // The job running fetches with the old inputs.
            dropJob(*e);
            e->fetchedAt = QDateTime();
        }
        fetchIfDue(itemId, true);
    }

    // Records the head files an item's job read, and watches them.
    void setHeads(StatusEntry &e, std::set<QString> heads)
    {
        e.heads = std::move(heads);
        watchHeads();
    }

    // Watches the head files that some entry read, and no others. A file Git
    // replaced is dropped by the watcher, so it is added again here.
    void watchHeads()
    {
        std::set<QString> wanted;
        for (const auto &[itemId, e] : entries)
            wanted.insert(e->heads.begin(), e->heads.end());
        const QStringList watched = headWatcher.files();
        QStringList stale;
        for (const QString &file : watched) {
            if (!wanted.count(file))
                stale << file;
        }
        if (!stale.isEmpty())
            headWatcher.removePaths(stale);
        QStringList missing;
        for (const QString &file : wanted) {
            if (!watched.contains(file) && QFileInfo::exists(file))
                missing << file;
        }
        if (!missing.isEmpty())
            headWatcher.addPaths(missing);
    }

    // A head file changed, as when a branch is checked out: the items whose
    // jobs read it are fetched again.
    void headChanged(const QString &file)
    {
        std::vector<QString> itemIds;
        for (const auto &[itemId, e] : entries) {
            if (e->heads.count(file))
                itemIds.push_back(itemId);
        }
        for (const QString &itemId : itemIds)
            fetchAgain(itemId);
        watchHeads();
    }

    // Empties an item's entry, keeping its subscribers and visibility.
    void empty(const QString &itemId)
    {
        StatusEntry *e = findEntry(itemId);
        if (!e)
            return;
        dropJob(*e);
        if (!e->heads.empty()) {
            e->heads.clear();
            watchHeads();
        }
        const bool had = e->status.state != ItemStatus::State::None || e->status.updatedAt.isValid();
        e->status = ItemStatus();
        e->fetchedAt = QDateTime();
        if (had) {
            emit q->statusChanged(itemId);
            e->notify();
        }
    }

    void dropJob(StatusEntry &e)
    {
        if (e.job) {
            e.job->abandon();
            e.job = nullptr;
        }
    }

    // The type of an item that reports status, or null: no such item, its
    // module is missing, or its type reports none.
    const ItemType *statusType(const Item &item) const
    {
        const ItemType *type = modules->findType(item.type);
        return type && type->hasStatus() ? type : nullptr;
    }

    // Whether the item is fetched on each poll. See the class comment.
    bool isDue(const Task &task, const Item &item) const
    {
        if (!statusType(item) || model->taskInTrash(task))
            return false;
        if (const StatusEntry *e = findEntry(item.id); e && e->visible > 0)
            return true;
        if (const auto it = openedTasks.find(task.id);
            it != openedTasks.end() && it->second.secsTo(now()) < openedTaskSecs)
            return true;
        // A watched item notifies while the window is hidden; decision 0087.
        return task.state == TaskState::Active
               && (item.watch || task.priority == Priority::Urgent || task.priority == Priority::High);
    }

    bool isStale(const QString &itemId) const
    {
        const StatusEntry *e = findEntry(itemId);
        if (!e || !e->fetchedAt.isValid())
            return true;
        return e->fetchedAt.secsTo(now()) >= settings->pollInterval();
    }

    // Fetches the item if it is due and stale, or at once if force is set.
    // force still needs an item of a type that reports status, out of the
    // trash.
    void fetchIfDue(const QString &itemId, bool onlyIfDue)
    {
        const Task *task = model->taskOfItem(itemId);
        const Item *item = task ? model->item(task->id, itemId) : nullptr;
        if (!item)
            return;
        if (onlyIfDue) {
            if (!isDue(*task, *item) || !isStale(itemId))
                return;
        } else if (!statusType(*item) || model->taskInTrash(*task)) {
            return;
        }
        start(*task, *item);
    }

    void start(const Task &task, const Item &item)
    {
        const ItemType *type = statusType(item);
        if (!type)
            return;
        StatusEntry &e = entry(item.id);
        if (e.job)
            return;   // one job per item

        const QString moduleId = moduleOf(item.type);
        const QString profileId = profiles::effective(*model, task.id, item.id);
        QJsonObject projectSettings;
        if (const Project *project = model->project(task.projectId))
            projectSettings = project->moduleSettings.value(moduleId).toObject();
        ApiClient *client = servers->client(profileId, moduleId);
        if (!client)
            client = &noServer;

        const QString itemId = item.id;
        auto *job = new StatusJobImpl(item.config, projectSettings, profileId, moduleId, foldersOf(task.projectId),
                                      *model, *client,
                                      [this, itemId](StatusJobImpl *done, const ItemStatus &status) {
                                          finished(itemId, done, status);
                                      });
        e.job = job;
        e.jobStarted = now();
        type->fetchStatus(*job);
    }

    void finished(const QString &itemId, StatusJobImpl *job, const ItemStatus &status)
    {
        StatusEntry *e = findEntry(itemId);
        if (!e || e->job != job)
            return;
        e->job = nullptr;
        setHeads(*e, job->heads());
        store(*e, status);
    }

    // Keeps a new status. An error keeps the data of the last good one.
    void store(StatusEntry &e, ItemStatus status)
    {
        const QDateTime time = now();
        e.fetchedAt = time;
        const bool error = status.state != ItemStatus::State::Ok && status.state != ItemStatus::State::None;
        if (status.state == ItemStatus::State::Ok && !status.updatedAt.isValid())
            status.updatedAt = time;
        if (error && e.status.updatedAt.isValid()) {
            ItemStatus kept = e.status;
            kept.state = status.state;
            kept.message = status.message;
            status = kept;
        }
        e.status = status;
        emit q->statusChanged(e.itemId);
        e.notify();
    }

    // Gives up on jobs that ran too long.
    void dropStuckJobs()
    {
        const QDateTime time = now();
        std::vector<StatusEntry *> stuck;
        for (const auto &[itemId, e] : entries) {
            if (e->job && e->jobStarted.secsTo(time) >= jobTimeoutSecs)
                stuck.push_back(e.get());
        }
        for (StatusEntry *e : stuck) {
            dropJob(*e);
            ItemStatus status;
            status.state = ItemStatus::State::Error;
            status.message = StatusCache::tr("The server did not answer in %1 seconds.").arg(jobTimeoutSecs);
            store(*e, status);
        }
    }

    void poll()
    {
        dropStuckJobs();
        const QDateTime time = now();
        for (auto it = openedTasks.begin(); it != openedTasks.end();) {
            if (it->second.secsTo(time) >= openedTaskSecs)
                it = openedTasks.erase(it);
            else
                ++it;
        }
        // Collected first: a type may finish at once, and the subscribers it
        // calls may change the model.
        std::vector<QString> due;
        forEachTask([&](const Task &task) {
            for (const Item &item : task.items) {
                if (isDue(task, item) && isStale(item.id))
                    due.push_back(item.id);
            }
        });
        for (const QString &itemId : due)
            fetchIfDue(itemId, true);
    }
};

StatusCache::StatusCache(Model &model, const ModuleDirectory &modules, ServerClients &servers,
                         DeviceSettings &settings, QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
    d->q = this;
    d->model = &model;
    d->modules = &modules;
    d->servers = &servers;
    d->settings = &settings;
    d->timer.setInterval(tickMs);
    connect(&d->timer, &QTimer::timeout, this, &StatusCache::poll);
    connect(&d->headWatcher, &QFileSystemWatcher::fileChanged, this,
            [this](const QString &file) { d->headChanged(file); });

    d->sync(false);
    connect(&model, &Model::modelReset, this, [this] { d->sync(false); });
    const auto changed = [this] { d->sync(true); };
    connect(&model, &Model::profilesChanged, this, changed);
    connect(&model, &Model::groupChanged, this, changed);
    connect(&model, &Model::groupRemoved, this, changed);
    connect(&model, &Model::projectAdded, this, changed);
    connect(&model, &Model::projectChanged, this, changed);
    connect(&model, &Model::projectRemoved, this, changed);
    connect(&model, &Model::taskAdded, this, changed);
    connect(&model, &Model::taskChanged, this, changed);
    connect(&model, &Model::taskRemoved, this, changed);
}

StatusCache::~StatusCache()
{
    for (const auto &[itemId, e] : d->entries)
        d->dropJob(*e);
}

ItemStatus StatusCache::status(const QString &itemId) const
{
    const StatusEntry *e = d->findEntry(itemId);
    return e ? e->status : ItemStatus();
}

ItemStatusSource &StatusCache::source(const QString &itemId)
{
    return d->entry(itemId);
}

void StatusCache::refresh(const QString &itemId)
{
    d->fetchIfDue(itemId, false);
}

void StatusCache::setVisible(const QString &itemId, bool visible)
{
    StatusEntry &e = d->entry(itemId);
    e.visible = std::max(0, e.visible + (visible ? 1 : -1));
    if (visible)
        d->fetchIfDue(itemId, true);
}

void StatusCache::noteTaskOpened(const QString &taskId)
{
    d->openedTasks[taskId] = d->now();
    const Task *task = d->model->task(taskId);
    if (!task)
        return;
    std::vector<QString> itemIds;
    for (const Item &item : task->items)
        itemIds.push_back(item.id);
    for (const QString &itemId : itemIds)
        d->fetchIfDue(itemId, true);
}

void StatusCache::poll()
{
    d->poll();
}

void StatusCache::setPolling(bool on)
{
    if (on)
        d->timer.start();
    else
        d->timer.stop();
}

void StatusCache::setClock(Clock clock)
{
    d->clock = std::move(clock);
}

void StatusCache::clear()
{
    d->openedTasks.clear();
    std::vector<QString> itemIds;
    for (const auto &[itemId, e] : d->entries)
        itemIds.push_back(itemId);
    for (const QString &itemId : itemIds)
        d->empty(itemId);
    d->known.clear();
    d->sync(false);
}

} // namespace mirante
