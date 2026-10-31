#include "ui/EntityActions.h"

#include "api/SecretStore.h"
#include "core/DeviceSettings.h"
#include "core/Edits.h"
#include "core/GroupColours.h"
#include "core/Model.h"
#include "core/Profiles.h"
#include "core/Store.h"
#include "items/FieldValidation.h"
#include "items/ItemTypeRegistry.h"
#include "items/UrlMatches.h"
#include "layout/Presets.h"
#include "ui/DashboardDialog.h"
#include "ui/FieldForm.h"
#include "ui/GroupDialog.h"
#include "ui/ImagePaste.h"
#include "ui/ImagePasteDialog.h"
#include "ui/ItemDialog.h"
#include "ui/ModuleSettingsDialog.h"
#include "ui/ProfilesDialog.h"
#include "ui/ProjectDialog.h"
#include "ui/TaskDialog.h"
#include "ui/WorkspaceFiles.h"
#include "web/ProfileManager.h"

#include <QActionGroup>
#include <QBuffer>
#include <QClipboard>
#include <QDateTime>
#include <QApplication>
#include <QDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
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
    std::function<QImage()> clipboardImage;
    std::function<bool(const QString &)> askFile;
    std::function<void(const QString &)> tellFile;
    SecretStore *secrets = nullptr;
    ImagePaste imagePaste{model, store, types};

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
        context.saveFile = [this](const QString &path) {
            return workspacefiles::copyIn(store, path, askFile, tellFile);
        };
        return context;
    }

    // Offers the workspace's profiles in a group dialog.
    void offerProfiles(GroupDialog &dialog) const
    {
        dialog.setProfiles(profiles::all(model), profiles::nameOf(model, model.defaultProfile()));
    }

    // Offers the workspace's profiles in an item dialog, naming the profile
    // of the task's group.
    void offerProfiles(ItemDialog &dialog, const QString &taskId) const
    {
        const Task *task = model.task(taskId);
        const Project *project = task ? model.project(task->projectId) : nullptr;
        const Group *group = project ? model.group(project->groupId) : nullptr;
        const QString groupProfile = group ? profiles::groupDefault(model, *group) : model.defaultProfile();
        dialog.setProfiles(profiles::all(model), groupProfile, profiles::nameOf(model, groupProfile));
    }

    // Runs the project dialog and applies its value and folder paths.
    bool runProjectDialog(const Project &project)
    {
        ProjectDialog dialog(project, model, settings, types, dialogParent);
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
    d->clipboardImage = [] {
        const QClipboard *clipboard = QGuiApplication::clipboard();
        return clipboard ? clipboard->image() : QImage();
    };
    // In front of the item dialog when its editor copies a file.
    const auto messageParent = [this]() -> QWidget * {
        QWidget *active = QApplication::activeWindow();
        return active ? active : d->dialogParent;
    };
    d->askFile = [messageParent](const QString &question) {
        return QMessageBox::question(messageParent(), EntityActions::tr("Keep File"), question) == QMessageBox::Yes;
    };
    d->tellFile = [messageParent](const QString &text) {
        QMessageBox::warning(messageParent(), EntityActions::tr("Keep File"), text);
    };
}

EntityActions::~EntityActions() = default;

void EntityActions::setExec(std::function<bool(QDialog &)> exec) { d->exec = std::move(exec); }

void EntityActions::setConfirm(std::function<bool(const QString &)> confirm) { d->confirm = std::move(confirm); }

void EntityActions::setModifiers(std::function<Qt::KeyboardModifiers()> modifiers)
{
    d->modifiers = std::move(modifiers);
}

void EntityActions::setClipboardImage(std::function<QImage()> clipboardImage)
{
    d->clipboardImage = std::move(clipboardImage);
}

void EntityActions::setFileMessages(std::function<bool(const QString &)> ask,
                                    std::function<void(const QString &)> tell)
{
    d->askFile = std::move(ask);
    d->tellFile = std::move(tell);
}

void EntityActions::setProfileManager(ProfileManager *manager) { d->profiles = manager; }

void EntityActions::setSecretStore(SecretStore *store) { d->secrets = store; }

void EntityActions::setEditProfiles(std::function<void(ProfilesDialog &)> edit)
{
    d->editProfiles = std::move(edit);
}

// Profiles.

void EntityActions::editProfiles()
{
    ProfilesDialog dialog(d->model, d->dialogParent);
    dialog.setConfirm(d->confirm);
    connect(&dialog, &ProfilesDialog::profileDeleted, this, [this](const QString &id) {
        // Its logins on this device go too. The placements that used it make
        // new views at once, but delete the old ones later. The profile goes
        // after those pages, or Qt warns.
        if (ProfileManager *profiles = d->profiles)
            QTimer::singleShot(0, profiles, [profiles, id] { profiles->removeProfile(id); });
        // Its tokens on this device go at once. The profile is already gone
        // from the model, so its keys are built here.
        if (SecretStore *secrets = d->secrets) {
            QStringList keys;
            for (const QString &moduleId : d->types.moduleIds()) {
                for (const SettingsScreen *screen : d->types.settingsScreens(moduleId)) {
                    for (const Field &field : screen->fields()) {
                        if (field.type == Field::Type::Secret)
                            keys << secretKey(d->model.workspaceId(), id, moduleId, field.key);
                    }
                }
            }
            secrets->removeAll(keys);
        }
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

void EntityActions::editModuleSettings(const QString &profileId, const QString &moduleId, const QString &screenId)
{
    const SettingsScreen *screen = d->types.settingsScreen(moduleId, screenId);
    if (!screen || !d->model.profile(profileId))
        return;
    ModuleSettingsDialog dialog(profiles::nameOf(d->model, profileId), d->types.moduleName(moduleId), *screen,
                                d->model.moduleSettings(profileId, moduleId, screenId), d->dialogParent);
    const QString workspaceId = d->model.workspaceId();
    auto *form = qobject_cast<FieldForm *>(dialog.editor());
    // The form says whether each token is set on this device, loading it
    // from the keychain if this session has not yet.
    if (form && d->secrets) {
        SecretStore *secrets = d->secrets;
        const auto stateOf = [secrets](const QString &key) {
            switch (secrets->state(key)) {
            case SecretStore::State::Set:
                return FieldForm::SecretState::Set;
            case SecretStore::State::NotSet:
                return FieldForm::SecretState::NotSet;
            case SecretStore::State::Unknown:
                break;
            }
            return FieldForm::SecretState::Unknown;
        };
        for (const Field &field : screen->fields()) {
            if (field.type != Field::Type::Secret)
                continue;
            const QString key = secretKey(workspaceId, profileId, moduleId, field.key);
            form->setSecretState(field.key, stateOf(key));
            if (secrets->state(key) == SecretStore::State::Unknown) {
                secrets->read(key, form, [form, stateOf, key, fieldKey = field.key](const std::optional<QString> &) {
                    form->setSecretState(fieldKey, stateOf(key));
                });
            }
        }
    }
    if (!d->exec(dialog))
        return;
    d->model.setModuleSettings(profileId, moduleId, screenId, dialog.values());
    // Tokens stay on this device, in the keychain. Without a secret store,
    // as in some tests, they are dropped.
    if (SecretStore *secrets = d->secrets) {
        if (form) {
            for (const QString &fieldKey : form->removedSecrets())
                secrets->remove(secretKey(workspaceId, profileId, moduleId, fieldKey));
        }
        // A pasted token often brings a line break along, which a server
        // refuses; no token has spaces at its ends.
        const QJsonObject typed = dialog.secrets();
        for (auto it = typed.begin(); it != typed.end(); ++it) {
            const QString token = it.value().toString().trimmed();
            if (!token.isEmpty())
                secrets->write(secretKey(workspaceId, profileId, moduleId, it.key()), token);
        }
    }
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
    // A second group often belongs to another account, so it may get a
    // profile of its own, named after it.
    if (!d->model.groups().empty()) {
        QStringList names;
        for (const Profile &profile : profiles::all(d->model))
            names << profile.name;
        dialog.offerNewProfile(names);
    }
    if (!d->exec(dialog))
        return {};
    Group created = dialog.value();
    if (dialog.createsProfile()) {
        Profile profile;
        profile.id = edits::newId();
        profile.name = dialog.newProfileName();
        d->model.setProfile(profile);
        created.profile = profile.id;
    }
    d->model.setGroup(created);
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

QString EntityActions::newItem(const QString &taskId, const QString &url)
{
    if (!d->model.task(taskId))
        return {};
    Item item;
    item.id = edits::newId();
    ItemDialog dialog(item, true, d->types, d->editorContext(taskId), d->dialogParent);
    d->offerProfiles(dialog, taskId);
    dialog.setUrl(url);
    if (!d->exec(dialog))
        return {};
    const Task *task = d->model.task(taskId);
    if (!task)
        return {};
    const Item value = dialog.value();
    d->model.setTask(edits::addItem(*task, value));
    return item.id;
}

QString EntityActions::pasteUrl(const QString &taskId)
{
    const QUrl url = clipboardUrl();
    if (url.isEmpty())
        return {};
    return newItem(taskId, url.toString());
}

QUrl EntityActions::clipboardUrl()
{
    const QClipboard *clipboard = QGuiApplication::clipboard();
    const QMimeData *data = clipboard ? clipboard->mimeData() : nullptr;
    if (!data)
        return {};
    if (data->hasUrls()) {
        for (const QUrl &url : data->urls()) {
            const QUrl address = urlmatches::webAddress(url.toString());
            if (!address.isEmpty())
                return address;
        }
    }
    return data->hasText() ? urlmatches::webAddress(data->text()) : QUrl();
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

QString EntityActions::pasteImage(const QString &taskId)
{
    return addImage(taskId, d->clipboardImage());
}

QString EntityActions::addImage(const QString &taskId, const QImage &image)
{
    if (image.isNull())
        return {};
    // A paste that cannot happen says why before any dialog opens.
    ImagePaste::Result result = d->imagePaste.checkTask(taskId);
    if (result == ImagePaste::Result::CreatedItem) {
        const QImage cropped = ImagePasteDialog::ask(image, d->dialogParent, d->exec);
        if (cropped.isNull())
            return {};
        QString itemId;
        result = d->imagePaste.addToTask(cropped, taskId, &itemId);
        if (result == ImagePaste::Result::CreatedItem)
            return itemId;
    }
    const QString text = ImagePaste::message(result);
    if (!text.isEmpty())
        emit message(text);
    return {};
}

QString EntityActions::addFile(const QString &taskId, const QString &path)
{
    const Task *task = d->model.task(taskId);
    if (!task)
        return {};
    if (d->store.readOnly()) {
        emit message(tr("This workspace is read-only, so no item can be added."));
        return {};
    }

    FileDrop drop;
    drop.path = QFileInfo(path).absoluteFilePath();
    if (const Project *project = d->model.project(task->projectId)) {
        for (const Folder &folder : project->folders) {
            const QString local = d->settings.folderPath(folder.id);
            if (!local.isEmpty())
                drop.folders.emplace_back(folder.id, local);
        }
    }

    for (const ItemType *type : d->types.types()) {
        const std::optional<FileMatch> match = type->fromFile(drop);
        if (!match)
            continue;
        bool copied = false;
        Item item;
        item.id = edits::newId();
        item.type = type->id();
        item.name = match->name.isEmpty() ? type->name() : match->name;
        item.config = defaultConfig(type->fields());
        for (auto it = match->config.constBegin(); it != match->config.constEnd(); ++it)
            item.config.insert(it.key(), it.value());
        if (!match->copyKey.isEmpty()) {
            const QString name = workspacefiles::copyIn(d->store, drop.path, d->askFile, d->tellFile);
            if (name.isEmpty())
                return {};
            item.config.insert(match->copyKey, name);
            // A file already in files/ keeps its name; a copy gets a new one.
            copied = name != QFileInfo(drop.path).fileName();
        }
        // A question above may have let the task change.
        const Task *current = d->model.task(taskId);
        if (!current)
            return {};
        d->model.setTask(edits::addItem(*current, item));
        // A copy is made silently otherwise, so say so.
        if (copied)
            emit message(tr("%1 was copied into the workspace.").arg(QFileInfo(drop.path).fileName()));
        return item.id;
    }
    emit message(tr("No item type can show this file."));
    return {};
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
    d->model.setItem(taskId, dialog.value());
}

void EntityActions::setItemWatched(const QString &taskId, const QString &itemId, bool watch)
{
    const Item *item = d->model.item(taskId, itemId);
    if (!item || item->watch == watch)
        return;
    Item changed = *item;
    changed.watch = watch;
    d->model.setItem(taskId, changed);
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

void EntityActions::moveItem(const QString &taskId, const QString &itemId, int index)
{
    if (const Task *task = d->model.task(taskId))
        d->model.setTask(edits::moveItem(*task, itemId, index));
}

void EntityActions::moveDashboard(const QString &taskId, const QString &dashboardId, int index)
{
    if (const Task *task = d->model.task(taskId))
        d->model.setTask(edits::moveDashboard(*task, dashboardId, index));
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

void EntityActions::addPasteImage(QMenu *menu, const QString &taskId)
{
    QAction *paste = menu->addAction(tr("Paste Image…"), this, [this, taskId] { pasteImage(taskId); });
    paste->setEnabled(!d->clipboardImage().isNull());
}

void EntityActions::addWatch(QMenu *menu, const QString &taskId, const QString &itemId)
{
    const Item *item = d->model.item(taskId, itemId);
    if (!item || !d->types.contains(item->type) || !d->types.type(item->type).hasStatus())
        return;
    const bool watched = item->watch;
    QAction *watch = menu->addAction(tr("Watch"), this,
                                     [this, taskId, itemId, watched] { setItemWatched(taskId, itemId, !watched); });
    watch->setCheckable(true);
    watch->setChecked(watched);
    watch->setToolTip(tr("Count on the tray icon, and notify"));
}

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
        menu->addAction(tr("Paste URL…"), this, [this, id] { pasteUrl(id); })->setEnabled(!clipboardUrl().isEmpty());
        addPasteImage(menu, id);
        menu->addAction(tr("Add Items from Link…"), this, [this, id] { emit addItemsFromLinkRequested(id); });
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
        menu->addAction(tr("Paste URL…"), this, [this, taskId] { pasteUrl(taskId); })
            ->setEnabled(!clipboardUrl().isEmpty());
        addPasteImage(menu, taskId);
        menu->addAction(tr("Add Items from Link…"), this, [this, taskId] { emit addItemsFromLinkRequested(taskId); });
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
        addWatch(menu, taskId, id);
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
