#include "items/ItemContextImpl.h"

#include "core/DeviceSettings.h"
#include "core/Model.h"
#include "core/Profiles.h"
#include "items/UnknownItemType.h"
#include "web/ProfileManager.h"
#include "web/WebPanel.h"

#include <mirante/ItemType.h>

#include <QCoreApplication>
#include <QLabel>

namespace mirante {

namespace {

QString tr(const char *text) { return QCoreApplication::translate("ItemContext", text); }

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
    return m_services.settings ? m_services.settings->folderPath(folderId) : QString();
}

QWidget *ItemContextImpl::createWebView(const QUrl &url, QWidget *parent)
{
    QWebEngineProfile *webProfile = profile();
    // Tests without a profile manager, or an item without a group, get a
    // message instead of a page.
    if (!webProfile)
        return createMessageView(tr("Web pages are not available here."), parent);
    auto *panel = new WebPanel(webProfile, url, parent);
    if (onHeaderWidget)
        onHeaderWidget(panel->toolbar());
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

// Stubs of T0. Task T8 of the M5 plan implements them.

ApiClient *ItemContextImpl::server()
{
    return nullptr;
}

ItemStatusSource *ItemContextImpl::status()
{
    return nullptr;
}

} // namespace mirante
