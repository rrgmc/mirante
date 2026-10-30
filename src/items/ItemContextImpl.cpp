#include "items/ItemContextImpl.h"

#include "api/ServerClient.h"
#include "api/StatusCache.h"
#include "core/DeviceSettings.h"
#include "core/Edits.h"
#include "core/GitHead.h"
#include "core/GitStatusReader.h"
#include "core/Model.h"
#include "core/Profiles.h"
#include "core/Store.h"
#include "items/ItemTypeRegistry.h"
#include "items/UnknownItemType.h"
#include "platform/Launcher.h"
#include "web/ProfileManager.h"
#include "web/WebPanel.h"

#include <mirante/ItemType.h>

#include <QApplication>
#include <QCoreApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>

#include <algorithm>

namespace mirante {

namespace {

QString tr(const char *text) { return QCoreApplication::translate("ItemContext", text); }

const Project *projectOfTask(const Model &model, const QString &taskId)
{
    const Task *task = model.task(taskId);
    return task ? model.project(task->projectId) : nullptr;
}

// The folder of a project by its id, or its main folder for an empty id.
const Folder *findFolder(const Project &project, const QString &folderId)
{
    for (const Folder &folder : project.folders) {
        if (folderId.isEmpty() ? folder.main : folder.id == folderId)
            return &folder;
    }
    return nullptr;
}

GitState toGitState(const GitStatus &status)
{
    GitState state;
    switch (status.result) {
    case GitStatus::Result::Ok:
        state.result = GitState::Result::Ok;
        break;
    case GitStatus::Result::NotARepository:
        state.result = GitState::Result::NotARepository;
        break;
    case GitStatus::Result::GitMissing:
        state.result = GitState::Result::GitMissing;
        break;
    case GitStatus::Result::Failed:
        state.result = GitState::Result::Failed;
        break;
    }
    state.branch = status.branch;
    state.commit = status.commit;
    state.hasUpstream = status.hasUpstream;
    state.ahead = status.ahead;
    state.behind = status.behind;
    state.staged = status.staged;
    state.changed = status.changed;
    state.untracked = status.untracked;
    state.conflicts = status.conflicts;
    state.message = status.message;
    return state;
}

} // namespace

QString moduleOfType(const QString &typeId) { return typeId.section(QLatin1Char('.'), 0, 0); }

QLabel *createMessageLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    label->setTextFormat(Qt::PlainText);
    label->setForegroundRole(QPalette::PlaceholderText);
    return label;
}

QString itemPlaceholderText(const ItemType *type, const QString &itemName, const QString &itemId)
{
    if (const auto *unknown = dynamic_cast<const UnknownItemType *>(type))
        return tr("This item needs the %1 module, which this build does not include.").arg(unknown->moduleId());
    return tr("Type: %1").arg(type ? type->name() : tr("unknown")) + QLatin1Char('\n') + tr("Name: %1").arg(itemName)
         + QLatin1Char('\n') + tr("Id: %1").arg(itemId);
}

ItemContextImpl::ItemContextImpl(const PlacementServices &services, const QString &taskId, const QString &itemId)
    : m_services(services)
    , m_taskId(taskId)
    , m_itemId(itemId)
{
}

ItemContextImpl::~ItemContextImpl() = default;

QString ItemContextImpl::itemName() const
{
    const Item *item = m_services.model ? m_services.model->item(m_taskId, m_itemId) : nullptr;
    return item ? item->name : QString();
}

QJsonObject ItemContextImpl::config() const
{
    const Item *item = m_services.model ? m_services.model->item(m_taskId, m_itemId) : nullptr;
    return item ? item->config : QJsonObject();
}

QJsonValue ItemContextImpl::projectSetting(const QString &key) const
{
    if (!m_services.model)
        return QJsonValue(QJsonValue::Undefined);
    const Item *item = m_services.model->item(m_taskId, m_itemId);
    const Task *task = m_services.model->task(m_taskId);
    const Project *project = task ? m_services.model->project(task->projectId) : nullptr;
    if (!item || !project)
        return QJsonValue(QJsonValue::Undefined);
    // A missing module or key gives undefined, as QJsonObject::value does.
    return project->moduleSettings.value(moduleOfType(item->type)).toObject().value(key);
}

QJsonObject ItemContextImpl::moduleSettings(const QString &screenId) const
{
    const Item *item = m_services.model ? m_services.model->item(m_taskId, m_itemId) : nullptr;
    if (!item)
        return {};
    const QString profile = profiles::effective(*m_services.model, m_taskId, m_itemId);
    return m_services.model->moduleSettings(profile, moduleOfType(item->type), screenId);
}

QString ItemContextImpl::folderPath(const QString &folderId) const
{
    if (!m_services.settings)
        return {};
    if (!folderId.isEmpty())
        return m_services.settings->folderPath(folderId);
    const Project *project = m_services.model ? projectOfTask(*m_services.model, m_taskId) : nullptr;
    const Folder *main = project ? findFolder(*project, QString()) : nullptr;
    return main ? m_services.settings->folderPath(main->id) : QString();
}

QWidget *ItemContextImpl::createWebView(const QUrl &url, QWidget *parent)
{
    QWidget *toolbar = nullptr;
    QWidget *page = createPage(url, parent, toolbar);
    if (toolbar && onHeaderWidget)
        onHeaderWidget(toolbar);
    return page;
}

QWidget *ItemContextImpl::createPage(const QUrl &url, QWidget *parent, QWidget *&toolbar)
{
    toolbar = nullptr;
    QWebEngineProfile *webProfile = profile();
    // Tests without a profile manager, or an item without a group, get a
    // message instead of a page.
    if (!webProfile)
        return createMessageView(tr("Web pages are not available here."), parent);
    auto *panel = new WebPanel(webProfile, url, parent);
    toolbar = panel->toolbar();
    return panel;
}

QWebEngineProfile *ItemContextImpl::profile()
{
    if (!m_services.profiles || !m_services.model)
        return nullptr;
    const Model &model = *m_services.model;
    const Item *item = model.item(m_taskId, m_itemId);
    const Task *task = model.task(m_taskId);
    const Project *project = task ? model.project(task->projectId) : nullptr;
    const Group *group = project ? model.group(project->groupId) : nullptr;
    if (!item || !group)
        return nullptr;
    return m_services.profiles->profile(profiles::effective(model, *item, group));
}

QWidget *ItemContextImpl::createMessageView(const QString &text, QWidget *parent)
{
    return createMessageLabel(text, parent);
}

void ItemContextImpl::updateConfig(const QJsonObject &config)
{
    if (!m_services.model)
        return;
    const Task *task = m_services.model->task(m_taskId);
    if (!task)
        return;
    Task changed = *task;
    for (Item &item : changed.items) {
        if (item.id == m_itemId) {
            item.config = config;
            m_services.model->setTask(changed);
            return;
        }
    }
}

void ItemContextImpl::showPage(const QUrl &url)
{
    if (onShowPage)
        onShowPage(url);
}

QString ItemContextImpl::addItemToTask(const QString &typeId, const QString &name, const QJsonObject &config)
{
    if (!m_services.model || !m_services.types || !m_services.types->contains(typeId))
        return {};
    const Task *task = m_services.model->task(m_taskId);
    if (!task)
        return {};
    // The profile stays empty, so the item follows the group.
    Item item;
    item.id = edits::newId();
    item.type = typeId;
    item.name = name;
    item.config = config;
    m_services.model->setTask(edits::addItem(*task, item));
    return item.id;
}

ApiClient *ItemContextImpl::server()
{
    const Item *item = m_services.model ? m_services.model->item(m_taskId, m_itemId) : nullptr;
    if (!item || !m_services.servers)
        return nullptr;
    return m_services.servers->client(profiles::effective(*m_services.model, m_taskId, m_itemId),
                                      moduleOfType(item->type));
}

ItemStatusSource *ItemContextImpl::status()
{
    const Item *item = m_services.model ? m_services.model->item(m_taskId, m_itemId) : nullptr;
    if (!item || !m_services.status || !m_services.types || !m_services.types->type(item->type).hasStatus())
        return nullptr;
    return &m_services.status->source(m_itemId);
}

void ItemContextImpl::mapFolder(const QString &folderId)
{
    if (!m_services.model || !m_services.settings)
        return;
    const Project *project = projectOfTask(*m_services.model, m_taskId);
    const Folder *folder = project ? findFolder(*project, folderId) : nullptr;
    if (!folder)
        return;
    // The dialog runs an event loop, in which the placement and this context
    // may be deleted, so nothing of this object is used after it.
    const PlacementServices services = m_services;
    const QString projectId = project->id;
    const QString id = folder->id;
    const QString path = QFileDialog::getExistingDirectory(QApplication::activeWindow(),
                                                           tr("Choose the folder \"%1\"").arg(folder->name),
                                                           services.settings->folderPath(id));
    if (path.isEmpty())
        return;
    services.settings->setFolderPath(id, path);
    // Statuses that read the folder, such as a pipeline that follows its
    // branch, are fetched again.
    if (services.status && services.model->project(projectId)) {
        std::vector<QString> itemIds;
        for (const Task *task : services.model->tasksOfProject(projectId)) {
            for (const Item &item : task->items)
                itemIds.push_back(item.id);
        }
        for (const QString &itemId : itemIds)
            services.status->refresh(itemId);
    }
}

void ItemContextImpl::onConfigChanged(QObject *receiver, std::function<void(const QJsonObject &)> changed)
{
    if (receiver && changed)
        m_configReceivers.push_back({receiver, std::move(changed)});
}

void ItemContextImpl::notifyConfigChanged(const QJsonObject &config)
{
    m_configReceivers.erase(std::remove_if(m_configReceivers.begin(), m_configReceivers.end(),
                                           [](const ConfigReceiver &r) { return r.receiver.isNull(); }),
                            m_configReceivers.end());
    // A receiver may register another, or be destroyed, while it is called,
    // so the list is copied first.
    const std::vector<ConfigReceiver> receivers = m_configReceivers;
    for (const ConfigReceiver &r : receivers) {
        if (r.receiver)
            r.changed(config);
    }
}

QJsonValue ItemContextImpl::deviceSetting(const QString &key) const
{
    const Item *item = m_services.model ? m_services.model->item(m_taskId, m_itemId) : nullptr;
    if (!item || !m_services.types)
        return QJsonValue(QJsonValue::Undefined);
    const QString moduleId = moduleOfType(item->type);
    for (const Field &field : m_services.types->deviceSettings(moduleId)) {
        if (field.key != key)
            continue;
        const QJsonValue stored = m_services.settings ? m_services.settings->moduleSetting(moduleId, key)
                                                      : QJsonValue(QJsonValue::Undefined);
        return stored.isUndefined() ? field.defaultValue : stored;
    }
    return QJsonValue(QJsonValue::Undefined);
}

// Only a plain file name is looked up: the name comes from synced data, and
// must not reach outside files/.
QString ItemContextImpl::filePath(const QString &fileName) const
{
    if (!m_services.store || fileName.isEmpty() || fileName == QLatin1String(".") || fileName == QLatin1String("..")
        || fileName.contains(QLatin1Char('/')) || fileName.contains(QLatin1Char('\\'))
        || fileName.contains(QLatin1Char(':')))
        return {};
    const QFileInfo info(m_services.store->filePath(fileName));
    return info.isFile() ? info.absoluteFilePath() : QString();
}

QString ItemContextImpl::gitHead(const QString &path) const
{
    return path.isEmpty() ? QString() : git::branchAt(path);
}

void ItemContextImpl::gitStatus(const QString &path, QObject *receiver, std::function<void(const GitState &)> done)
{
    if (!m_services.git || !receiver || !done)
        return;
    m_services.git->read(path, receiver,
                         [done = std::move(done)](const GitStatus &status) { done(toGitState(status)); });
}

QString ItemContextImpl::launchTerminal(const QString &dir, const QString &program, const QStringList &arguments)
{
    const QString commandTemplate = m_services.settings ? m_services.settings->terminalCommand() : QString();
    return launch::launchTerminal(commandTemplate, dir, program, arguments);
}

QString ItemContextImpl::startProgram(const QString &dir, const QString &program, const QStringList &arguments)
{
    return launch::startProgram(dir, program, arguments);
}

void ItemContextImpl::showFolder(const QString &path)
{
    if (!path.isEmpty())
        launch::showFolder(path);
}

} // namespace mirante
