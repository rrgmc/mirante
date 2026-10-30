#include "items/DashBridge.h"

#include "api/Attention.h"
#include "api/StatusCache.h"
#include "core/Model.h"
#include "core/Sorting.h"
#include "items/ItemTypeRegistry.h"

#include <QApplication>
#include <QColor>
#include <QEvent>
#include <QFont>
#include <QFontInfo>
#include <QPalette>
#include <QTimer>
#include <QWidget>

#include <algorithm>

namespace mirante {

namespace {

QString priorityName(Priority priority)
{
    switch (priority) {
    case Priority::Urgent: return DashBridge::tr("Urgent");
    case Priority::High: return DashBridge::tr("High");
    case Priority::Normal: return DashBridge::tr("Normal");
    case Priority::Low: return DashBridge::tr("Low");
    }
    return {};
}

QString stateName(TaskState state)
{
    switch (state) {
    case TaskState::Active: return QStringLiteral("active");
    case TaskState::Done: return QStringLiteral("done");
    case TaskState::Archived: return QStringLiteral("archived");
    }
    return {};
}

QString statusStateName(ItemStatus::State state)
{
    switch (state) {
    case ItemStatus::State::None: return QStringLiteral("none");
    case ItemStatus::State::Ok: return QStringLiteral("ok");
    case ItemStatus::State::NoServer: return QStringLiteral("noserver");
    case ItemStatus::State::NoToken: return QStringLiteral("notoken");
    case ItemStatus::State::Refused: return QStringLiteral("refused");
    case ItemStatus::State::NotFound: return QStringLiteral("notfound");
    case ItemStatus::State::Error: return QStringLiteral("error");
    }
    return {};
}

// A colour as CSS takes it: "#rrggbb", or rgba() when it is see-through.
QString cssColour(const QColor &colour)
{
    if (colour.alpha() == 255)
        return colour.name(QColor::HexRgb);
    return QStringLiteral("rgba(%1, %2, %3, %4)")
        .arg(colour.red())
        .arg(colour.green())
        .arg(colour.blue())
        .arg(colour.alphaF(), 0, 'f', 2);
}

// The font's size in CSS pixels. Without a font database, as on the
// offscreen platform, it comes from the point size.
int fontPixels(const QFont &font)
{
    const int pixels = QFontInfo(font).pixelSize();
    if (pixels > 0)
        return pixels;
    return font.pointSizeF() > 0 ? qRound(font.pointSizeF() * 4 / 3) : 13;
}

} // namespace

struct DashBridge::Private
{
    PlacementServices services;
    QString taskId;
    bool changePending = false;
    // What the panels show of attention when it was last read, so a change
    // of counts elsewhere in the workspace emits nothing.
    QStringList shownAttention;

    const Task *task() const { return services.model ? services.model->task(taskId) : nullptr; }

    const Project *project() const
    {
        const Task *found = task();
        return found ? services.model->project(found->projectId) : nullptr;
    }

    QString groupId() const
    {
        const Project *found = project();
        return found ? found->groupId : QString();
    }

    bool ownsItem(const QString &itemId) const
    {
        return !itemId.isEmpty() && services.model && services.model->item(taskId, itemId);
    }

    // The attention a panel shows: of the task's items, and the counts of
    // the group's tasks.
    QStringList attentionShown() const
    {
        QStringList shown;
        const Task *own = task();
        if (!own || !services.attention || !services.model)
            return shown;
        for (const Item &item : own->items) {
            for (const AttentionReason &reason : services.attention->reasons(item.id))
                shown << item.id + QLatin1Char(':') + reason.text;
        }
        const QString group = groupId();
        for (const Project *project : services.model->projectsOfGroup(group)) {
            for (const Task *other : services.model->tasksOfProject(project->id))
                shown << other->id + QLatin1Char('=') + QString::number(services.attention->taskCount(other->id));
        }
        return shown;
    }

    // True for a task whose project is in the panel's group.
    bool inGroup(const QString &otherTaskId) const
    {
        const Task *other = services.model ? services.model->task(otherTaskId) : nullptr;
        const Project *project = other ? services.model->project(other->projectId) : nullptr;
        return project && !project->groupId.isEmpty() && project->groupId == groupId();
    }
};

DashBridge::DashBridge(const PlacementServices &services, const QString &taskId, QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
    d->services = services;
    d->taskId = taskId;

    // Every change that a panel may show: its task, the tasks, projects and
    // name of its group, the status of its items, and the attention counts.
    // A removed entity can no longer be looked up, so its removal always counts.
    auto schedule = [this] {
        if (d->changePending)
            return;
        d->changePending = true;
        QTimer::singleShot(0, this, [this] {
            d->changePending = false;
            emit changed();
        });
    };
    if (Model *model = services.model) {
        auto taskTouched = [this, schedule](const QString &id) {
            if (id == d->taskId || d->inGroup(id))
                schedule();
        };
        auto projectTouched = [this, schedule, model](const QString &id) {
            const Project *project = model->project(id);
            if (!project || project->groupId == d->groupId())
                schedule();
        };
        auto groupTouched = [this, schedule](const QString &id) {
            if (id == d->groupId())
                schedule();
        };
        connect(model, &Model::modelReset, this, schedule);
        connect(model, &Model::taskAdded, this, taskTouched);
        connect(model, &Model::taskChanged, this, taskTouched);
        connect(model, &Model::taskRemoved, this, schedule);
        connect(model, &Model::projectAdded, this, projectTouched);
        connect(model, &Model::projectChanged, this, projectTouched);
        connect(model, &Model::projectRemoved, this, schedule);
        connect(model, &Model::groupChanged, this, groupTouched);
    }
    if (StatusCache *status = services.status) {
        connect(status, &StatusCache::statusChanged, this, [this, schedule](const QString &itemId) {
            if (d->ownsItem(itemId))
                schedule();
        });
    }
    if (Attention *attention = services.attention) {
        d->shownAttention = d->attentionShown();
        connect(attention, &Attention::countsChanged, this, [this, schedule] {
            QStringList shown = d->attentionShown();
            if (shown == d->shownAttention)
                return;
            d->shownAttention = std::move(shown);
            schedule();
        });
    }
}

DashBridge::~DashBridge() = default;

void DashBridge::followPalette(QWidget *widget)
{
    if (widget)
        widget->installEventFilter(this);
}

bool DashBridge::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange) {
        if (!d->changePending) {
            d->changePending = true;
            QTimer::singleShot(0, this, [this] {
                d->changePending = false;
                emit changed();
            });
        }
    }
    return QObject::eventFilter(watched, event);
}

QJsonObject DashBridge::currentTask() const
{
    const Task *task = d->task();
    if (!task)
        return {};
    const Project *project = d->project();
    const Group *group = project ? d->services.model->group(project->groupId) : nullptr;

    QJsonArray items;
    for (const Item &item : task->items) {
        const ItemTypeRegistry *types = d->services.types;
        const bool known = types && types->contains(item.type);
        items.append(QJsonObject{
            {QStringLiteral("id"), item.id},
            {QStringLiteral("name"), item.name},
            {QStringLiteral("type"), item.type},
            {QStringLiteral("typeName"), known ? types->type(item.type).shortName() : QString()},
            {QStringLiteral("hasStatus"), known && types->type(item.type).hasStatus()},
        });
    }
    return {
        {QStringLiteral("id"), task->id},
        {QStringLiteral("title"), task->title},
        {QStringLiteral("project"), project ? project->name : QString()},
        {QStringLiteral("group"), group ? group->name : QString()},
        {QStringLiteral("groupId"), group ? group->id : QString()},
        {QStringLiteral("priority"), int(task->priority)},
        {QStringLiteral("priorityName"), priorityName(task->priority)},
        {QStringLiteral("state"), stateName(task->state)},
        {QStringLiteral("items"), items},
    };
}

QJsonObject DashBridge::itemStatus(const QString &itemId) const
{
    if (!d->ownsItem(itemId))
        return {};
    if (!d->services.status)
        return {{QStringLiteral("state"), QStringLiteral("none")}};
    const ItemStatus status = d->services.status->status(itemId);
    QJsonArray reasons;
    bool needsAttention = false;
    if (Attention *attention = d->services.attention) {
        for (const AttentionReason &reason : attention->reasons(itemId))
            reasons.append(reason.text);
        needsAttention = attention->needsAttention(itemId);
    }
    QJsonObject json{
        {QStringLiteral("state"), statusStateName(status.state)},
        {QStringLiteral("summary"), status.summary},
        {QStringLiteral("message"), status.message},
        {QStringLiteral("updated"), status.updatedAt.isValid() ? statusAge(status.updatedAt) : QString()},
        {QStringLiteral("attention"), reasons},
        {QStringLiteral("needsAttention"), needsAttention},
    };
    json.insert(QStringLiteral("closed"), status.closed ? QJsonValue(*status.closed) : QJsonValue());
    return json;
}

QJsonArray DashBridge::groupTasks(const QString &groupId) const
{
    if (groupId.isEmpty() || groupId != d->groupId())
        return {};
    const Model &model = *d->services.model;
    // The sidebar's order of active tasks not in the trash, then by priority.
    std::vector<const Task *> tasks;
    for (const Project *project : sortedProjects(model.projectsOfGroup(groupId), SortFilter())) {
        for (const Task *task : sortedTasks(model.tasksOfProject(project->id), SortFilter()))
            tasks.push_back(task);
    }
    std::stable_sort(tasks.begin(), tasks.end(),
                     [](const Task *a, const Task *b) { return int(a->priority) < int(b->priority); });

    QJsonArray list;
    for (const Task *task : tasks) {
        const Project *project = model.project(task->projectId);
        const int count = d->services.attention ? d->services.attention->taskCount(task->id) : 0;
        list.append(QJsonObject{
            {QStringLiteral("id"), task->id},
            {QStringLiteral("title"), task->title},
            {QStringLiteral("project"), project ? project->name : QString()},
            {QStringLiteral("priority"), int(task->priority)},
            {QStringLiteral("priorityName"), priorityName(task->priority)},
            {QStringLiteral("attention"), count},
            {QStringLiteral("attentionText"), count > 0 ? tr("%n item(s) need attention", nullptr, count) : QString()},
            {QStringLiteral("current"), task->id == d->taskId},
        });
    }
    return list;
}

void DashBridge::openItem(const QString &itemId)
{
    if (d->ownsItem(itemId) && d->services.openItem)
        d->services.openItem(d->taskId, itemId);
}

void DashBridge::openTask(const QString &taskId)
{
    if (d->inGroup(taskId) && d->services.model && !d->services.model->taskInTrash(taskId) && d->services.openTask)
        d->services.openTask(taskId);
}

QJsonObject DashBridge::texts() const
{
    return {
        {QStringLiteral("noStatusItems"), tr("No items in this task report a status.")},
        {QStringLiteral("noActiveTasks"), tr("No active tasks in this group.")},
        {QStringLiteral("notFetched"), tr("Not fetched yet")},
        {QStringLiteral("closed"), tr("Closed")},
        {QStringLiteral("thisTask"), tr("This task")},
        {QStringLiteral("taskGone"), tr("This panel's task no longer exists.")},
    };
}

QJsonObject DashBridge::palette() const
{
    const QPalette palette = QApplication::palette();
    const QFont font = QApplication::font();
    // The priority and attention colours of the sidebar (SidebarDelegate.cpp).
    QColor normal(0x3b, 0x82, 0xf6);
    normal.setAlphaF(0.35f);
    return {
        {QStringLiteral("window"), cssColour(palette.color(QPalette::Window))},
        {QStringLiteral("base"), cssColour(palette.color(QPalette::Base))},
        {QStringLiteral("alternateBase"), cssColour(palette.color(QPalette::AlternateBase))},
        {QStringLiteral("text"), cssColour(palette.color(QPalette::WindowText))},
        {QStringLiteral("muted"), cssColour(palette.color(QPalette::PlaceholderText))},
        {QStringLiteral("border"), cssColour(palette.color(QPalette::Mid))},
        {QStringLiteral("accent"), cssColour(palette.color(QPalette::Accent))},
        {QStringLiteral("highlight"), cssColour(palette.color(QPalette::Highlight))},
        {QStringLiteral("link"), cssColour(palette.color(QPalette::Link))},
        {QStringLiteral("attention"), cssColour(QColor(0xd9, 0x48, 0x3b))},
        {QStringLiteral("priorityUrgent"), cssColour(QColor(0xd9, 0x3b, 0x3b))},
        {QStringLiteral("priorityHigh"), cssColour(QColor(0xe8, 0x86, 0x2a))},
        {QStringLiteral("priorityNormal"), cssColour(normal)},
        {QStringLiteral("priorityLow"), cssColour(QColor(0x9a, 0xa1, 0xad))},
        {QStringLiteral("fontFamily"), QFontInfo(font).family()},
        {QStringLiteral("fontSize"), fontPixels(font)},
    };
}

} // namespace mirante
