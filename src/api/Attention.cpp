#include "api/Attention.h"

#include "api/StatusCache.h"
#include "core/DeviceSettings.h"
#include "core/Model.h"

#include <QTimer>

namespace mirante {

namespace {

// Whether a status holds data from the server, so its reasons are real. An
// item whose first fetch failed has none.
bool hasData(const ItemStatus &status)
{
    return status.updatedAt.isValid();
}

QStringList clearsOnViewKeys(const ItemStatus &status)
{
    QStringList keys;
    for (const AttentionReason &reason : status.attention) {
        if (reason.clearsOnView && !keys.contains(reason.key))
            keys << reason.key;
    }
    return keys;
}

} // namespace

struct Attention::Private
{
    Attention *q = nullptr;
    const Model *model = nullptr;
    StatusCache *status = nullptr;
    DeviceSettings *settings = nullptr;
    bool countsPending = false;

    // Emits countsChanged once, on the next pass of the event loop.
    void scheduleCounts()
    {
        if (countsPending)
            return;
        countsPending = true;
        QTimer::singleShot(0, q, [this] {
            countsPending = false;
            emit q->countsChanged();
        });
    }

    // The first status with data records its clearsOnView keys as seen, so
    // what was already true when the item was added does not count.
    void onStatus(const QString &itemId)
    {
        const QString workspaceId = model->workspaceId();
        const ItemStatus current = status->status(itemId);
        if (hasData(current) && !workspaceId.isEmpty() && !settings->hasSeenAttention(workspaceId, itemId))
            settings->setSeenAttention(workspaceId, itemId, clearsOnViewKeys(current));
        scheduleCounts();
    }

    int taskCount(const Task &task) const
    {
        if (task.state != TaskState::Active || model->taskInTrash(task))
            return 0;
        int count = 0;
        for (const Item &item : task.items) {
            if (q->needsAttention(item.id))
                ++count;
        }
        return count;
    }

    int projectCount(const Project &project) const
    {
        if (project.isTrashed())
            return 0;
        int count = 0;
        for (const Task *task : model->tasksOfProject(project.id))
            count += taskCount(*task);
        return count;
    }
};

Attention::Attention(const Model &model, StatusCache &status, DeviceSettings &settings, QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
    d->q = this;
    d->model = &model;
    d->status = &status;
    d->settings = &settings;

    connect(&status, &StatusCache::statusChanged, this, [this](const QString &itemId) { d->onStatus(itemId); });
    // A task's state, priority, trash or place changes what it counts.
    const auto counts = [this] { d->scheduleCounts(); };
    connect(&model, &Model::modelReset, this, counts);
    connect(&model, &Model::groupRemoved, this, counts);
    connect(&model, &Model::projectChanged, this, counts);
    connect(&model, &Model::projectRemoved, this, counts);
    connect(&model, &Model::taskAdded, this, counts);
    connect(&model, &Model::taskChanged, this, counts);
    connect(&model, &Model::taskRemoved, this, counts);
}

Attention::~Attention() = default;

// Without a workspace id there is nowhere to keep seen keys, so reasons that
// clear on view do not count. Without a record yet, they do not count either:
// the record is about to be made with them, as onStatus() does.
QList<AttentionReason> Attention::reasons(const QString &itemId) const
{
    const ItemStatus current = d->status->status(itemId);
    const QString workspaceId = d->model->workspaceId();
    const bool recorded = !workspaceId.isEmpty() && d->settings->hasSeenAttention(workspaceId, itemId);
    const QStringList seen = recorded ? d->settings->seenAttention(workspaceId, itemId) : QStringList();
    QList<AttentionReason> counting;
    for (const AttentionReason &reason : current.attention) {
        if (!reason.clearsOnView || (recorded && !seen.contains(reason.key)))
            counting << reason;
    }
    return counting;
}

bool Attention::needsAttention(const QString &itemId) const
{
    return !reasons(itemId).isEmpty();
}

int Attention::taskCount(const QString &taskId) const
{
    const Task *task = d->model->task(taskId);
    return task ? d->taskCount(*task) : 0;
}

int Attention::projectCount(const QString &projectId) const
{
    const Project *project = d->model->project(projectId);
    return project ? d->projectCount(*project) : 0;
}

int Attention::groupCount(const QString &groupId) const
{
    int count = 0;
    for (const Project *project : d->model->projectsOfGroup(groupId))
        count += d->projectCount(*project);
    return count;
}

// The record becomes the current clearsOnView keys, so keys no longer
// reported drop out of it. A status without data leaves the record alone: the
// item's first real status must still record what it finds.
void Attention::markSeen(const QString &itemId)
{
    const QString workspaceId = d->model->workspaceId();
    const ItemStatus current = d->status->status(itemId);
    if (workspaceId.isEmpty() || !hasData(current))
        return;
    const QStringList keys = clearsOnViewKeys(current);
    if (d->settings->hasSeenAttention(workspaceId, itemId) && d->settings->seenAttention(workspaceId, itemId) == keys)
        return;
    d->settings->setSeenAttention(workspaceId, itemId, keys);
    d->scheduleCounts();
}

} // namespace mirante
