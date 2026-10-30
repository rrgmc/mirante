#include "items/QuickAddRun.h"

#include "api/ServerClient.h"
#include "core/Edits.h"
#include "core/LayoutTree.h"
#include "core/Model.h"
#include "core/Sorting.h"
#include "items/ItemTypeRegistry.h"

#include <QJsonValue>
#include <QPointer>
#include <QTimer>

#include <algorithm>
#include <functional>
#include <utility>

namespace mirante {

namespace {

// The sidebar's order of groups: by order, then by name, ignoring case.
bool groupBefore(const Group *a, const Group *b)
{
    if (a->order != b->order)
        return a->order < b->order;
    if (const int c = QString::compare(a->name, b->name, Qt::CaseInsensitive); c != 0)
        return c < 0;
    return a->id < b->id;
}

// Whether a stored project setting equals the value a handler gave. Strings
// compare without case. A setting that is not stored matches nothing.
bool settingMatches(const QJsonValue &setting, const QJsonValue &value)
{
    if (setting.isUndefined() || setting.isNull())
        return false;
    if (setting.isString() && value.isString())
        return QString::compare(setting.toString(), value.toString(), Qt::CaseInsensitive) == 0;
    return setting == value;
}

bool sameItem(const QuickAddItem &a, const QuickAddItem &b)
{
    return a.typeId == b.typeId && a.config == b.config;
}

bool isOffered(const std::vector<QuickAddItem> &items, const QuickAddItem &item)
{
    return std::any_of(items.begin(), items.end(), [&item](const QuickAddItem &other) { return sameItem(other, item); });
}

// What both kinds of request share: the module's settings and server in a
// profile, the owner, the timeout, and finishing exactly once. The request is
// its own owner, so deleting it cuts the replies a module bound to owner().
// It deletes itself, later, once it has finished, timed out or been dropped.
class RequestBase : public QObject
{
public:
    RequestBase(Model &model, ServerClients *servers, const QString &profileId, const QString &moduleId,
                QObject *parent)
        : QObject(parent)
        , m_model(model)
        , m_servers(servers)
        , m_profileId(profileId)
        , m_moduleId(moduleId)
    {
    }

    QJsonObject settings(const QString &screenId) const
    {
        return m_model.moduleSettings(m_profileId, m_moduleId, screenId);
    }

    ApiClient *client() { return m_servers ? m_servers->client(m_profileId, m_moduleId) : nullptr; }

    // Starts the timeout. When it runs out, the request ends as if it had
    // found nothing.
    void startTimer(int ms)
    {
        m_timer.setSingleShot(true);
        QObject::connect(&m_timer, &QTimer::timeout, this, [this] { timedOut(); });
        m_timer.start(ms);
    }

    // Drops the request: its answer is not wanted any more.
    void drop()
    {
        if (m_ended)
            return;
        m_ended = true;
        m_timer.stop();
        deleteLater();
    }

    bool ended() const { return m_ended; }

protected:
    // Marks the request ended and schedules its deletion. False if it had
    // already ended, for a module that finishes twice or after the timeout.
    bool end()
    {
        if (m_ended)
            return false;
        m_ended = true;
        m_timer.stop();
        deleteLater();
        return true;
    }

    virtual void timedOut() = 0;

    Model &model() const { return m_model; }
    QString moduleId() const { return m_moduleId; }

private:
    Model &m_model;
    ServerClients *m_servers = nullptr;
    QString m_profileId;
    QString m_moduleId;
    QTimer m_timer;
    bool m_ended = false;
};

class HandlerRequest : public RequestBase, public QuickAddRequest
{
public:
    using Done = std::function<void(std::optional<QuickAddProposal>)>;

    HandlerRequest(Model &model, ServerClients *servers, const QString &profileId, const QString &moduleId,
                   const QString &text, Done done, QObject *parent)
        : RequestBase(model, servers, profileId, moduleId, parent)
        , m_text(text)
        , m_done(std::move(done))
    {
    }

    QString text() const override { return m_text; }
    QJsonObject moduleSettings(const QString &screenId) const override { return settings(screenId); }
    ApiClient *server() override { return client(); }
    QObject *owner() override { return this; }

    void finish(std::optional<QuickAddProposal> proposal) override
    {
        if (!end())
            return;
        if (Done done = std::exchange(m_done, nullptr))
            done(std::move(proposal));
    }

protected:
    void timedOut() override { finish(std::nullopt); }

private:
    QString m_text;
    Done m_done;
};

class FinderRequest : public RequestBase, public RelatedItemRequest
{
public:
    using Done = std::function<void(std::vector<QuickAddItem>)>;
    using Offered = std::function<std::vector<QuickAddItem>()>;

    FinderRequest(Model &model, ServerClients *servers, const QString &profileId, const QString &moduleId,
                  const QStringList &texts, const QString &projectId, Offered offered, Done done,
                  QObject *parent)
        : RequestBase(model, servers, profileId, moduleId, parent)
        , m_texts(texts)
        , m_projectId(projectId)
        , m_offered(std::move(offered))
        , m_done(std::move(done))
    {
    }

    QStringList texts() const override { return m_texts; }
    std::vector<QuickAddItem> items() const override { return m_offered ? m_offered() : std::vector<QuickAddItem>(); }

    QJsonValue projectSetting(const QString &key) const override
    {
        const Project *project = model().project(m_projectId);
        if (!project)
            return QJsonValue(QJsonValue::Undefined);
        return project->moduleSettings.value(moduleId()).toObject().value(key);
    }

    QJsonObject moduleSettings(const QString &screenId) const override { return settings(screenId); }
    ApiClient *server() override { return client(); }
    QObject *owner() override { return this; }

    void finish(std::vector<QuickAddItem> items) override
    {
        if (!end())
            return;
        m_offered = nullptr;
        if (Done done = std::exchange(m_done, nullptr))
            done(std::move(items));
    }

protected:
    void timedOut() override { finish({}); }

private:
    QStringList m_texts;
    QString m_projectId;
    Offered m_offered;
    Done m_done;
};

} // namespace

struct QuickAddRun::Private
{
    QuickAddRun *q = nullptr;
    Model &model;
    const ItemTypeRegistry &types;
    ServerClients *servers = nullptr;
    int timeout = 30000;
    QString taskId;         // the task the items go to, or empty

    // Each new run gets a new number, so answers of an older one are dropped.
    int proposeRun = 0;
    int findRun = 0;
    int proposing = 0;      // handlers still running
    int finding = 0;        // finders still running
    std::vector<QPointer<RequestBase>> requests;

    // Everything offered in the current finder run: the proposal's items,
    // then what each finder added.
    std::vector<QuickAddItem> offered;

    Private(QuickAddRun *q, Model &model, const ItemTypeRegistry &types, ServerClients *servers)
        : q(q)
        , model(model)
        , types(types)
        , servers(servers)
    {
    }

    void dropRequests()
    {
        for (const QPointer<RequestBase> &request : requests) {
            if (request)
                request->drop();
        }
        requests.clear();
    }

    void forgetEnded()
    {
        requests.erase(std::remove_if(requests.begin(), requests.end(),
                                      [](const QPointer<RequestBase> &r) { return !r || r->ended(); }),
                       requests.end());
    }

    // The items the task already has, as quick add items, to leave out.
    std::vector<QuickAddItem> taskItems() const
    {
        std::vector<QuickAddItem> items;
        if (const Task *task = taskId.isEmpty() ? nullptr : model.task(taskId)) {
            for (const Item &item : task->items)
                items.push_back(QuickAddItem{item.type, item.name, item.config});
        }
        return items;
    }

    void stopProposing()
    {
        ++proposeRun;
        proposing = 0;
    }

    void stopFinding()
    {
        ++findRun;
        finding = 0;
    }

    void handlerDone(int run, const QString &moduleId, std::optional<QuickAddProposal> proposal)
    {
        if (run != proposeRun)
            return;
        --proposing;
        if (proposal) {
            const std::vector<QuickAddItem> had = taskItems();
            std::vector<QuickAddItem> &items = proposal->items;
            items.erase(std::remove_if(items.begin(), items.end(),
                                       [&had](const QuickAddItem &item) { return isOffered(had, item); }),
                        items.end());
            emit q->proposed(Proposal{moduleId, std::move(*proposal)});
        }
        // A slot may have started another run.
        if (run == proposeRun && proposing == 0)
            emit q->proposalsDone();
    }

    void finderDone(int run, std::vector<QuickAddItem> items)
    {
        if (run != findRun)
            return;
        --finding;
        std::vector<QuickAddItem> fresh;
        for (QuickAddItem &item : items) {
            if (isOffered(offered, item))
                continue;
            offered.push_back(item);
            fresh.push_back(std::move(item));
        }
        if (!fresh.empty())
            emit q->found(fresh);
        if (run == findRun && finding == 0)
            emit q->findingDone();
    }
};

QuickAddRun::QuickAddRun(Model &model, const ItemTypeRegistry &types, ServerClients *servers, QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>(this, model, types, servers))
{
}

QuickAddRun::~QuickAddRun()
{
    // The requests are children, and go with this object. Their answers must
    // not reach d while it goes.
    d->stopProposing();
    d->stopFinding();
    d->dropRequests();
}

void QuickAddRun::setTimeout(int ms) { d->timeout = ms; }

void QuickAddRun::setTask(const QString &taskId) { d->taskId = taskId; }

void QuickAddRun::propose(const QString &text, const QString &profileId)
{
    // A new reading makes the related items of the old one stale too.
    d->stopProposing();
    d->stopFinding();
    d->dropRequests();

    const int run = d->proposeRun;
    const std::vector<ItemTypeRegistry::ModuleHandler> handlers = d->types.quickAddHandlers();
    const QString trimmed = text.trimmed();
    // One more than the handlers until all have started, so that handlers
    // that finish at once do not end the run early.
    d->proposing = int(handlers.size()) + 1;
    for (const ItemTypeRegistry::ModuleHandler &handler : handlers) {
        const QString moduleId = handler.moduleId;
        auto *request = new HandlerRequest(
            d->model, d->servers, profileId, moduleId, trimmed,
            [this, run, moduleId](std::optional<QuickAddProposal> proposal) {
                d->handlerDone(run, moduleId, std::move(proposal));
            },
            this);
        d->requests.emplace_back(request);
        request->startTimer(d->timeout);
        handler.handler->propose(*request);
        if (run != d->proposeRun)
            return;
    }
    d->forgetEnded();
    --d->proposing;
    if (d->proposing == 0)
        emit proposalsDone();
}

QStringList QuickAddRun::matchProjects(const Model &model, const QString &moduleId,
                                       const std::optional<ProjectMatch> &match)
{
    if (!match || match->key.isEmpty())
        return {};
    QStringList ids;
    std::vector<const Group *> groups = model.groups();
    std::sort(groups.begin(), groups.end(), groupBefore);
    for (const Group *group : groups) {
        for (const Project *project : sortedProjects(model.projectsOfGroup(group->id), SortFilter())) {
            const QJsonValue setting = project->moduleSettings.value(moduleId).toObject().value(match->key);
            if (settingMatches(setting, match->value))
                ids << project->id;
        }
    }
    return ids;
}

void QuickAddRun::findRelated(const Proposal &proposal, const QString &title, const QString &projectId,
                              const QString &profileId)
{
    d->stopFinding();
    // Only the finders' requests go; handlers still running keep going.
    for (const QPointer<RequestBase> &request : d->requests) {
        if (dynamic_cast<FinderRequest *>(request.data()))
            request->drop();
    }
    d->forgetEnded();

    const int run = d->findRun;
    // The task's items count as offered, so the finders see them and what
    // they find again is dropped.
    d->offered = d->taskItems();
    for (const QuickAddItem &item : proposal.proposal.items) {
        if (!isOffered(d->offered, item))
            d->offered.push_back(item);
    }
    QStringList texts = proposal.proposal.texts;
    if (!title.trimmed().isEmpty())
        texts << title.trimmed();

    const std::vector<ItemTypeRegistry::ModuleFinder> finders = d->types.relatedItemFinders();
    d->finding = int(finders.size()) + 1;
    for (const ItemTypeRegistry::ModuleFinder &finder : finders) {
        auto *request = new FinderRequest(
            d->model, d->servers, profileId, finder.moduleId, texts, projectId, [this] { return d->offered; },
            [this, run](std::vector<QuickAddItem> items) { d->finderDone(run, std::move(items)); }, this);
        d->requests.emplace_back(request);
        request->startTimer(d->timeout);
        finder.finder->find(*request);
        if (run != d->findRun)
            return;
    }
    d->forgetEnded();
    --d->finding;
    if (d->finding == 0)
        emit findingDone();
}

void QuickAddRun::cancel()
{
    d->stopProposing();
    d->stopFinding();
    d->dropRequests();
}

bool QuickAddRun::busy() const { return d->proposing > 0 || d->finding > 0; }

QString QuickAddRun::create(const QString &projectId, const QString &title, const std::vector<QuickAddItem> &items)
{
    if (!d->model.project(projectId))
        return {};

    Task task = edits::newTask(projectId, title);
    int order = -1;
    for (const Task *sibling : d->model.tasksOfProject(projectId))
        order = std::max(order, sibling->order);
    task.order = order + 1;
    task.state = TaskState::Active;
    task.priority = Priority::Normal;

    // The items, in order, and one cell that holds them: a single item fills
    // the cell, several are its tabs, the first one current.
    LayoutNode tabs;
    tabs.kind = LayoutNode::Kind::Tabs;
    tabs.current = 0;
    for (const QuickAddItem &offeredItem : items) {
        Item item;
        item.id = edits::newId();
        item.type = offeredItem.typeId;
        item.name = offeredItem.name;
        item.config = offeredItem.config;
        task = edits::addItem(task, item);

        LayoutNode cell;
        cell.kind = LayoutNode::Kind::Cell;
        cell.ref = CellRef{item.id, {}, {}};
        tabs.children.push_back(cell);
        tabs.titles.push_back(item.name);
    }
    if (tabs.children.size() == 1) {
        task.dashboards.front().layout = tabs.children.front();
    } else if (!tabs.children.empty()) {
        task.dashboards.front().layout = tabs;
    }

    d->model.setTask(task);
    return task.id;
}

QStringList QuickAddRun::addToTask(const QString &taskId, const std::vector<QuickAddItem> &items)
{
    const Task *existing = d->model.task(taskId);
    if (!existing)
        return {};
    Task task = *existing;
    QStringList ids;
    for (const QuickAddItem &offeredItem : items) {
        Item item;
        item.id = edits::newId();
        item.type = offeredItem.typeId;
        item.name = offeredItem.name;
        item.config = offeredItem.config;
        task = edits::addItem(task, item);
        ids << item.id;
    }
    if (!ids.isEmpty())
        d->model.setTask(task);
    return ids;
}

} // namespace mirante
