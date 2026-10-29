#include "ui/EntityActions.h"

#include "core/DeviceSettings.h"
#include "core/Edits.h"
#include "core/GroupColours.h"
#include "core/Model.h"
#include "core/Profiles.h"
#include "core/Store.h"
#include "items/ItemTypeRegistry.h"
#include "layout/Presets.h"
#include "ui/ConnectionsDialog.h"
#include "ui/DashboardDialog.h"
#include "ui/GroupDialog.h"
#include "ui/ItemDialog.h"
#include "ui/ModuleSettingsDialog.h"
#include "ui/ProfilesDialog.h"
#include "ui/ProjectDialog.h"
#include "ui/TaskDialog.h"
#include "web/ProfileManager.h"

#include <QActionGroup>
#include <QBuffer>
#include <QDateTime>
#include <QDialog>
#include <QGuiApplication>
#include <QImage>
#include <QMenu>
#include <QMessageBox>
#include <QTimer>

#include <algorithm>
#include <climits>

namespace mirante {

namespace {

// The order after the last of the entries: the place of a new project, task
// or group, at the end of its siblings whatever its priority.
template <typename T>
int orderAfterLast(const std::vector<const T *> &entries)
{
    int order = -1;
    for (const T *entry : entries)
        order = std::max(order, entry->order);
    return order + 1;
}

template <typename T>
std::vector<T> copies(const std::vector<const T *> &entries)
{
    std::vector<T> values;
    values.reserve(entries.size());
    for (const T *entry : entries)
        values.push_back(*entry);
    return values;
}

// "1 task", "4 tasks". Spelled out so the text reads well without a translation.
QString countText(int count, const QString &one, const QString &many)
{
    return count == 1 ? one : many.arg(count);
}

QString priorityName(Priority priority)
{
    switch (priority) {
    case Priority::Urgent:
        return EntityActions::tr("Urgent");
    case Priority::High:
        return EntityActions::tr("High");
    case Priority::Normal:
        return EntityActions::tr("Normal");
    case Priority::Low:
        return EntityActions::tr("Low");
    }
    return {};
}

QString stateName(TaskState state)
{
    switch (state) {
    case TaskState::Active:
        return EntityActions::tr("Active");
    case TaskState::Done:
        return EntityActions::tr("Done");
    case TaskState::Archived:
        return EntityActions::tr("Archived");
    }
    return {};
}

} // namespace

struct EntityActions::Private
{
    Model &model;
    Store &store;
    const ItemTypeRegistry &types;
    DeviceSettings &settings;
    QWidget *dialogParent = nullptr;
    std::function<bool(QDialog &)> exec;
    std::function<bool(const QString &)> confirm;
    std::function<Qt::KeyboardModifiers()> modifiers;
    ProfileManager *profiles = nullptr;
    std::function<void(ProfilesDialog &)> editProfiles;

    bool shiftHeld() const { return modifiers && (modifiers() & Qt::ShiftModifier); }

    static QString now() { return QDateTime::currentDateTimeUtc().toString(Qt::ISODate); }

    Private(Model &model, Store &store, const ItemTypeRegistry &types, DeviceSettings &settings,
            QWidget *dialogParent)
        : model(model)
        , store(store)
        , types(types)
        , settings(settings)
        , dialogParent(dialogParent)
    {
    }

    EditorContext editorContext(const QString &taskId) const
    {
        EditorContext context;
        if (const Task *task = model.task(taskId)) {
            if (const Project *project = model.project(task->projectId)) {
                for (const Folder &folder : project->folders)
                    context.folders.emplace_back(folder.id, folder.name);
            }
        }
        Store *s = &store;
        context.saveImage = [s](const QImage &image) {
            QByteArray bytes;
            QBuffer buffer(&bytes);
            buffer.open(QIODevice::WriteOnly);
            if (!image.save(&buffer, "PNG"))
                return QString();
            return s->saveFile(bytes, QStringLiteral("png"));
        };
        context.loadImage = [s](const QString &name) { return QImage(s->filePath(name)); };
        return context;
    }

    // Offers the workspace's profiles in a group dialog.
    void offerProfiles(GroupDialog &dialog) const { dialog.setProfiles(profiles::all(model)); }

    // Offers the workspace's profiles in an item dialog, naming the profile
    // of the task's group.
    void offerProfiles(ItemDialog &dialog, const QString &taskId) const
    {
        const Task *task = model.task(taskId);
        const Project *project = task ? model.project(task->projectId) : nullptr;
        const Group *group = project ? model.group(project->groupId) : nullptr;
        const QString groupProfile = group ? profiles::groupDefault(model, *group) : kDefaultProfile;
        dialog.setProfiles(profiles::all(model), groupProfile, profiles::nameOf(model, groupProfile));
    }

    // Runs the project dialog and applies its value and folder paths.
    bool runProjectDialog(const Project &project)
    {
        ProjectDialog dialog(project, model, settings, dialogParent);
        if (!exec(dialog))
            return false;
        const QMap<QString, QString> paths = dialog.folderPaths();
        for (auto it = paths.cbegin(); it != paths.cend(); ++it)
            settings.setFolderPath(it.key(), it.value());
        model.setProject(dialog.value());
        return true;
    }
};

EntityActions::EntityActions(Model &model, Store &store, const ItemTypeRegistry &types, DeviceSettings &settings,
                             QWidget *dialogParent)
    : d(std::make_unique<Private>(model, store, types, settings, dialogParent))
{
    d->exec = [](QDialog &dialog) { return dialog.exec() == QDialog::Accepted; };
    d->confirm = [this](const QString &question) {
        return QMessageBox::question(d->dialogParent, tr("Delete"), question) == QMessageBox::Yes;
    };
    d->modifiers = [] { return QGuiApplication::queryKeyboardModifiers(); };
}

EntityActions::~EntityActions() = default;

void EntityActions::setExec(std::function<bool(QDialog &)> exec) { d->exec = std::move(exec); }

void EntityActions::setConfirm(std::function<bool(const QString &)> confirm) { d->confirm = std::move(confirm); }

void EntityActions::setModifiers(std::function<Qt::KeyboardModifiers()> modifiers)
{
    d->modifiers = std::move(modifiers);
}

void EntityActions::setProfileManager(ProfileManager *manager) { d->profiles = manager; }

void EntityActions::setEditProfiles(std::function<void(ProfilesDialog &)> edit)
{
    d->editProfiles = std::move(edit);
}

// Profiles.

void EntityActions::editProfiles()
{
    ProfilesDialog dialog(d->model, d->dialogParent);
    dialog.setConfirm(d->confirm);
    dialog.setConnectionKinds(d->types.connectionKinds());
    dialog.setEditConnections([this](ConnectionsDialog &connections) { return d->exec(connections); });
    connect(&dialog, &ProfilesDialog::profileDeleted, this, [this](const QString &id) {
        // Its logins on this device go too. The placements that used it make
        // new views at once, but delete the old ones later. The profile goes
        // after those pages, or Qt warns.
        if (ProfileManager *profiles = d->profiles)
            QTimer::singleShot(0, profiles, [profiles, id] { profiles->removeProfile(id); });
    });
    connect(&dialog, &ProfilesDialog::profileClearRequested, this, [this](const QString &id) {
        if (ProfileManager *profiles = d->profiles)
            profiles->clearProfile(id);
    });
    if (d->editProfiles)
        d->editProfiles(dialog);
    else
        dialog.exec();
}

void EntityActions::editModuleSettings(const QString &moduleId, const QString &screenId)
{
    const SettingsScreen *screen = d->types.settingsScreen(moduleId, screenId);
    if (!screen)
        return;
    Model *model = &d->model;
    ModuleSettingsDialog dialog(d->types.moduleName(moduleId), *screen, profiles::all(*model),
                                [model, moduleId, screenId](const QString &profileId) {
                                    return model->moduleSettings(profileId, moduleId, screenId);
                                },
                                d->dialogParent);
    if (!d->exec(dialog))
        return;
    for (const auto &[profileId, values] : dialog.values())
        d->model.setModuleSettings(profileId, moduleId, screenId, values);
}

// Creating.

QString EntityActions::newGroup()
{
    Group group;
    group.id = edits::newId();
    group.order = orderAfterLast(d->model.groups());
    // A new group takes the next palette colour no group uses, and stores it.
    group.colour = groupcolours::next(d->model);
    GroupDialog dialog(group, d->dialogParent);
    d->offerProfiles(dialog);
    if (!d->exec(dialog))
        return {};
    d->model.setGroup(dialog.value());
    return group.id;
}

QString EntityActions::newProject(const QString &groupId)
{
    if (!d->model.group(groupId))
        return {};
    Project project;
    project.id = edits::newId();
    project.groupId = groupId;
    project.order = orderAfterLast(d->model.projectsOfGroup(groupId));
    return d->runProjectDialog(project) ? project.id : QString();
}

QString EntityActions::newTask(const QString &projectId)
{
    if (!d->model.project(projectId))
        return {};
    Task task = edits::newTask(projectId, QString());
    task.order = orderAfterLast(d->model.tasksOfProject(projectId));
    TaskDialog dialog(task, d->model, d->dialogParent);
    if (!d->exec(dialog))
        return {};
    d->model.setTask(dialog.value());
    return task.id;
}

QString EntityActions::newItem(const QString &taskId)
{
    if (!d->model.task(taskId))
        return {};
    Item item;
    item.id = edits::newId();
    ItemDialog dialog(item, true, d->types, d->editorContext(taskId), d->dialogParent);
    d->offerProfiles(dialog, taskId);
    if (!d->exec(dialog))
        return {};
    const Task *task = d->model.task(taskId);
    if (!task)
        return {};
    d->model.setTask(edits::addItem(*task, dialog.value()));
    return item.id;
}

QString EntityActions::newDashboard(const QString &taskId)
{
    if (!d->model.task(taskId))
        return {};
    Dashboard dashboard;
    dashboard.id = edits::newId();
    DashboardDialog dialog(dashboard, true, d->dialogParent);
    if (!d->exec(dialog))
        return {};
    const Task *task = d->model.task(taskId);
    if (!task)
        return {};
    QStringList itemIds;
    QStringList itemNames;
    for (const Item &item : task->items) {
        itemIds << item.id;
        itemNames << item.name;
    }
    dashboard = dialog.value();
    dashboard.layout = presets::build(std::max(dialog.presetIndex(), 0), itemIds, itemNames);
    d->model.setTask(edits::addDashboard(*task, dashboard));
    emit dashboardCreated(taskId, dashboard.id);
    return dashboard.id;
}

// Editing.

void EntityActions::editGroup(const QString &id)
{
    const Group *group = d->model.group(id);
    if (!group)
        return;
    GroupDialog dialog(*group, d->dialogParent);
    d->offerProfiles(dialog);
    if (d->exec(dialog))
        d->model.setGroup(dialog.value());
}

void EntityActions::editProject(const QString &id)
{
    if (const Project *project = d->model.project(id))
        d->runProjectDialog(*project);
}

void EntityActions::editTask(const QString &id)
{
    const Task *task = d->model.task(id);
    if (!task)
        return;
    TaskDialog dialog(*task, d->model, d->dialogParent);
    if (d->exec(dialog))
        d->model.setTask(dialog.value());
}

void EntityActions::editItem(const QString &taskId, const QString &itemId)
{
    const Item *item = d->model.item(taskId, itemId);
    if (!item)
        return;
    ItemDialog dialog(*item, false, d->types, d->editorContext(taskId), d->dialogParent);
    d->offerProfiles(dialog, taskId);
    if (!d->exec(dialog))
        return;
    const Task *task = d->model.task(taskId);
    if (!task)
        return;
    Task changed = *task;
    const Item value = dialog.value();
    for (Item &entry : changed.items) {
        if (entry.id == itemId)
            entry = value;
    }
    d->model.setTask(changed);
}

void EntityActions::renameDashboard(const QString &taskId, const QString &dashboardId)
{
    const Dashboard *dashboard = d->model.taskDashboard(taskId, dashboardId);
    if (!dashboard)
        return;
    DashboardDialog dialog(*dashboard, false, d->dialogParent);
    if (!d->exec(dialog))
        return;
    const Task *task = d->model.task(taskId);
    if (!task)
        return;
    Task changed = *task;
    const QString name = dialog.value().name;
    for (Dashboard &entry : changed.dashboards) {
        if (entry.id == dashboardId)
            entry.name = name;
    }
    d->model.setTask(changed);
}

// The trash.

void EntityActions::trashProject(const QString &id)
{
    const Project *project = d->model.project(id);
    if (!project || project->isTrashed())
        return;
    Project changed = *project;
    changed.trashedAt = Private::now();
    d->model.setProject(changed);
}

void EntityActions::trashTask(const QString &id)
{
    const Task *task = d->model.task(id);
    if (!task || task->isTrashed())
        return;
    Task changed = *task;
    changed.trashedAt = Private::now();
    d->model.setTask(changed);
}

void EntityActions::restoreProject(const QString &id)
{
    const Project *project = d->model.project(id);
    if (!project || !project->isTrashed())
        return;
    Project changed = *project;
    changed.trashedAt.clear();
    d->model.setProject(changed);
}

// The project first, so the task never shows under a project in the trash.
void EntityActions::restoreTask(const QString &id)
{
    const Task *task = d->model.task(id);
    if (!task)
        return;
    restoreProject(task->projectId);
    task = d->model.task(id);
    if (!task || !task->isTrashed())
        return;
    Task changed = *task;
    changed.trashedAt.clear();
    d->model.setTask(changed);
}

int EntityActions::trashedProjectCount() const
{
    int count = 0;
    for (const Group *group : d->model.groups()) {
        for (const Project *project : d->model.projectsOfGroup(group->id))
            count += project->isTrashed() ? 1 : 0;
    }
    return count;
}

int EntityActions::trashedTaskCount() const
{
    int count = 0;
    for (const Group *group : d->model.groups()) {
        for (const Project *project : d->model.projectsOfGroup(group->id)) {
            for (const Task *task : d->model.tasksOfProject(project->id))
                count += d->model.taskInTrash(*task) ? 1 : 0;
        }
    }
    return count;
}

void EntityActions::emptyTrash()
{
    const int projectCount = trashedProjectCount();
    const int taskCount = trashedTaskCount();
    if (projectCount == 0 && taskCount == 0)
        return;
    const QString projectsText = countText(projectCount, tr("1 project"), tr("%1 projects"));
    const QString tasksText = countText(taskCount, tr("1 task"), tr("%1 tasks"));
    QString what;
    if (projectCount == 0)
        what = tasksText;
    else if (taskCount == 0)
        what = projectsText;
    else
        what = tr("%1 and %2").arg(projectsText, tasksText);
    if (!d->confirm(tr("Permanently delete %1 in the trash?").arg(what)))
        return;

    // The ids first, since each removal changes the model. A project takes
    // its tasks with it.
    QStringList projectIds;
    QStringList taskIds;
    for (const Group *group : d->model.groups()) {
        for (const Project *project : d->model.projectsOfGroup(group->id)) {
            if (project->isTrashed()) {
                projectIds.append(project->id);
                continue;
            }
            for (const Task *task : d->model.tasksOfProject(project->id)) {
                if (task->isTrashed())
                    taskIds.append(task->id);
            }
        }
    }
    for (const QString &id : taskIds)
        d->model.removeTask(id);
    for (const QString &id : projectIds)
        d->model.removeProject(id);
}

// Deleting.

void EntityActions::removeGroup(const QString &id)
{
    const Group *group = d->model.group(id);
    if (!group)
        return;
    const auto projects = d->model.projectsOfGroup(id);
    int taskCount = 0;
    for (const Project *project : projects)
        taskCount += static_cast<int>(d->model.tasksOfProject(project->id).size());
    const int projectCount = static_cast<int>(projects.size());

    QString question;
    if (projectCount == 0) {
        question = tr("Delete group %1?").arg(group->name);
    } else {
        const QString projectsText = countText(projectCount, tr("1 project"), tr("%1 projects"));
        if (taskCount == 0) {
            question = tr("Delete group %1 and its %2?").arg(group->name, projectsText);
        } else {
            const QString tasksText = countText(taskCount, tr("1 task"), tr("%1 tasks"));
            question = tr("Delete group %1, its %2 and %3?").arg(group->name, projectsText, tasksText);
        }
    }
    if (!d->confirm(question))
        return;
    d->model.removeGroup(id);
}

void EntityActions::removeProject(const QString &id)
{
    const Project *project = d->model.project(id);
    if (!project)
        return;
    const int taskCount = static_cast<int>(d->model.tasksOfProject(id).size());
    const QString question = taskCount == 0
        ? tr("Delete project %1?").arg(project->name)
        : tr("Delete project %1 and its %2?").arg(project->name, countText(taskCount, tr("1 task"), tr("%1 tasks")));
    if (d->confirm(question))
        d->model.removeProject(id);
}

void EntityActions::removeTask(const QString &id)
{
    const Task *task = d->model.task(id);
    if (!task)
        return;
    if (d->confirm(tr("Delete task %1?").arg(task->title)))
        d->model.removeTask(id);
}

void EntityActions::removeItem(const QString &taskId, const QString &itemId)
{
    const Task *task = d->model.task(taskId);
    const Item *item = d->model.item(taskId, itemId);
    if (!task || !item)
        return;
    if (!d->confirm(tr("Delete item %1 from task %2?").arg(item->name, task->title)))
        return;
    if (const Task *current = d->model.task(taskId))
        d->model.setTask(edits::removeItem(*current, itemId));
}

void EntityActions::removeDashboard(const QString &taskId, const QString &dashboardId)
{
    const Task *task = d->model.task(taskId);
    const Dashboard *dashboard = d->model.taskDashboard(taskId, dashboardId);
    if (!task || !dashboard || task->dashboards.size() <= 1)
        return;
    if (!d->confirm(tr("Delete dashboard %1?").arg(dashboard->name)))
        return;
    if (const Task *current = d->model.task(taskId))
        d->model.setTask(edits::removeDashboard(*current, dashboardId));
}

// Quick changes. A new priority changes the marker only; the entry keeps its place.

void EntityActions::setPriority(const QString &projectOrTaskId, Priority priority)
{
    if (const Project *project = d->model.project(projectOrTaskId)) {
        Project changed = *project;
        changed.priority = priority;
        d->model.setProject(changed);
    } else if (const Task *task = d->model.task(projectOrTaskId)) {
        Task changed = *task;
        changed.priority = priority;
        d->model.setTask(changed);
    }
}

void EntityActions::setState(const QString &taskId, TaskState state)
{
    const Task *task = d->model.task(taskId);
    if (!task || task->state == state)
        return;
    Task changed = *task;
    changed.state = state;
    d->model.setTask(changed);
}

void EntityActions::setDefaultDashboard(const QString &taskId, const QString &dashboardId)
{
    if (const Task *task = d->model.task(taskId))
        d->model.setTask(edits::setDefaultDashboard(*task, dashboardId));
}

// Moves.

void EntityActions::moveGroup(const QString &id, int index)
{
    for (const Group &group : edits::moveGroup(copies(d->model.groups()), id, index))
        d->model.setGroup(group);
}

void EntityActions::moveProject(const QString &id, int index)
{
    const Project *project = d->model.project(id);
    if (!project)
        return;
    const auto siblings = copies(d->model.projectsOfGroup(project->groupId));
    for (const Project &changed : edits::moveProject(siblings, id, index))
        d->model.setProject(changed);
}

void EntityActions::moveTask(const QString &id, int index)
{
    const Task *task = d->model.task(id);
    if (!task)
        return;
    const auto siblings = copies(d->model.tasksOfProject(task->projectId));
    for (const Task &changed : edits::moveTask(siblings, id, index))
        d->model.setTask(changed);
}

// Menus.

namespace {

void addPriorityMenu(QMenu *menu, EntityActions *actions, const QString &id, Priority current)
{
    QMenu *sub = menu->addMenu(EntityActions::tr("Priority"));
    auto *group = new QActionGroup(sub);
    for (Priority priority : {Priority::Urgent, Priority::High, Priority::Normal, Priority::Low}) {
        QAction *action = sub->addAction(priorityName(priority),
                                         [actions, id, priority] { actions->setPriority(id, priority); });
        action->setCheckable(true);
        action->setChecked(priority == current);
        group->addAction(action);
    }
}

void addStateMenu(QMenu *menu, EntityActions *actions, const QString &taskId, TaskState current)
{
    QMenu *sub = menu->addMenu(EntityActions::tr("State"));
    auto *group = new QActionGroup(sub);
    for (TaskState state : {TaskState::Active, TaskState::Done, TaskState::Archived}) {
        QAction *action = sub->addAction(stateName(state),
                                         [actions, taskId, state] { actions->setState(taskId, state); });
        action->setCheckable(true);
        action->setChecked(state == current);
        group->addAction(action);
    }
}

} // namespace

void EntityActions::fillMenu(QMenu *menu, SidebarModel::Kind kind, const QString &id, const QString &taskId)
{
    if (!menu)
        return;
    const Model &model = d->model;
    switch (kind) {
    case SidebarModel::Kind::Group:
        if (!model.group(id))
            return;
        menu->addAction(tr("New project…"), this, [this, id] { newProject(id); });
        menu->addAction(tr("New group…"), this, [this] { newGroup(); });
        menu->addSeparator();
        menu->addAction(tr("Edit group…"), this, [this, id] { editGroup(id); });
        menu->addSeparator();
        menu->addAction(tr("Delete group…"), this, [this, id] { removeGroup(id); });
        break;
    case SidebarModel::Kind::Project: {
        const Project *project = model.project(id);
        if (!project)
            return;
        menu->addAction(tr("New task…"), this, [this, id] { newTask(id); });
        menu->addSeparator();
        menu->addAction(tr("Edit project…"), this, [this, id] { editProject(id); });
        addPriorityMenu(menu, this, id, project->priority);
        menu->addSeparator();
        // Shift held when the command runs deletes for good, after a confirmation.
        menu->addAction(tr("Delete project…"), this, [this, id] {
            if (d->shiftHeld())
                removeProject(id);
            else
                trashProject(id);
        });
        break;
    }
    case SidebarModel::Kind::Task: {
        const Task *task = model.task(id);
        if (!task)
            return;
        menu->addAction(tr("New item…"), this, [this, id] { newItem(id); });
        menu->addAction(tr("New dashboard…"), this, [this, id] { newDashboard(id); });
        menu->addSeparator();
        menu->addAction(tr("Edit task…"), this, [this, id] { editTask(id); });
        addPriorityMenu(menu, this, id, task->priority);
        addStateMenu(menu, this, id, task->state);
        menu->addSeparator();
        menu->addAction(tr("Delete task…"), this, [this, id] {
            if (d->shiftHeld())
                removeTask(id);
            else
                trashTask(id);
        });
        break;
    }
    case SidebarModel::Kind::ItemList:
        if (!model.task(taskId))
            return;
        menu->addAction(tr("New item…"), this, [this, taskId] { newItem(taskId); });
        break;
    case SidebarModel::Kind::DashboardList:
        if (!model.task(taskId))
            return;
        menu->addAction(tr("New dashboard…"), this, [this, taskId] { newDashboard(taskId); });
        break;
    case SidebarModel::Kind::Item:
        if (!model.item(taskId, id))
            return;
        menu->addAction(tr("Edit item…"), this, [this, taskId, id] { editItem(taskId, id); });
        menu->addSeparator();
        menu->addAction(tr("Delete item…"), this, [this, taskId, id] { removeItem(taskId, id); });
        break;
    case SidebarModel::Kind::Dashboard: {
        const Task *task = model.task(taskId);
        if (!task || !model.taskDashboard(taskId, id))
            return;
        menu->addAction(tr("Rename dashboard…"), this, [this, taskId, id] { renameDashboard(taskId, id); });
        QAction *setDefault =
            menu->addAction(tr("Set as default"), this, [this, taskId, id] { setDefaultDashboard(taskId, id); });
        setDefault->setEnabled(task->defaultDashboardId != id);
        menu->addSeparator();
        QAction *remove =
            menu->addAction(tr("Delete dashboard…"), this, [this, taskId, id] { removeDashboard(taskId, id); });
        remove->setEnabled(task->dashboards.size() > 1);
        break;
    }
    }
}

void EntityActions::fillTrashMenu(QMenu *menu, SidebarModel::Kind kind, const QString &id,
                                  const QString & /*taskId*/)
{
    if (!menu)
        return;
    if (kind == SidebarModel::Kind::Project && d->model.project(id)) {
        menu->addAction(tr("Restore"), this, [this, id] { restoreProject(id); });
        menu->addSeparator();
        menu->addAction(tr("Delete permanently…"), this, [this, id] { removeProject(id); });
    } else if (kind == SidebarModel::Kind::Task && d->model.task(id)) {
        menu->addAction(tr("Restore"), this, [this, id] { restoreTask(id); });
        menu->addSeparator();
        menu->addAction(tr("Delete permanently…"), this, [this, id] { removeTask(id); });
    }
}

} // namespace mirante
