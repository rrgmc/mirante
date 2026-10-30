#pragma once

// What tst_sidebar_model and tst_focus_view share for attention badges: a
// small workspace whose items are of a type that reports status, and a real
// Attention over a StatusCache. A test gives an item a status with
// setReasons(), and the counts follow.

#include "api/Attention.h"
#include "api/ModuleDirectory.h"
#include "api/SecretBackend.h"
#include "api/SecretStore.h"
#include "api/ServerClient.h"
#include "api/StatusCache.h"
#include "core/DeviceSettings.h"
#include "core/Model.h"

#include <mirante/ItemType.h>

#include <QSettings>
#include <QTemporaryDir>

#include <map>
#include <memory>

namespace mirante::attentiontest {

// Finishes each fetch with the status given for the item, which it finds by
// the item id kept in the configuration as "id".
class StatusType : public ItemType
{
public:
    QString id() const override { return QStringLiteral("test.status"); }
    QString name() const override { return QStringLiteral("Status"); }
    QIcon icon() const override { return {}; }
    bool hasStatus() const override { return true; }
    void fetchStatus(StatusJob &job) const override
    {
        job.finish(next[job.config().value(QStringLiteral("id")).toString()]);
    }

    mutable std::map<QString, ItemStatus> next;
};

class Directory : public ModuleDirectory
{
public:
    QStringList moduleIds() const override { return {QStringLiteral("test")}; }
    const ItemType *findType(const QString &typeId) const override
    {
        return typeId == type.id() ? &type : nullptr;
    }
    const ServerType *serverType(const QString &) const override { return nullptr; }
    const ModuleSettings *moduleSettings(const QString &) const override { return nullptr; }
    std::vector<const SettingsScreen *> settingsScreens(const QString &) const override { return {}; }

    StatusType type;
};

inline Item makeItem(const QString &id)
{
    Item item;
    item.id = id;
    item.type = QStringLiteral("test.status");
    item.name = id;
    item.config.insert(QStringLiteral("id"), id);
    return item;
}

inline Task makeTask(const QString &id, const QString &projectId, Priority priority, int order,
                     std::vector<Item> items, TaskState state = TaskState::Active)
{
    Task task;
    task.id = id;
    task.projectId = projectId;
    task.title = id;
    task.priority = priority;
    task.order = order;
    task.state = state;
    task.items = std::move(items);
    return task;
}

// The workspace "ws":
//   group g1: project p1, with
//     t1 High:            i1, i2
//     t2 Normal, order 0: i3
//     t3 Normal, order 1: i4
//     t4 Urgent, done:    i5
//   group g2: project p2, with
//     t5 Normal:          i6
struct Fixture
{
    Fixture()
    {
        settingsFile = std::make_unique<QSettings>(dir.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
        device = std::make_unique<DeviceSettings>(settingsFile.get());

        Group g1;
        g1.id = QStringLiteral("g1");
        g1.name = QStringLiteral("G1");
        g1.order = 0;
        Group g2;
        g2.id = QStringLiteral("g2");
        g2.name = QStringLiteral("G2");
        g2.order = 1;
        Project p1;
        p1.id = QStringLiteral("p1");
        p1.groupId = g1.id;
        p1.name = QStringLiteral("P1");
        Project p2;
        p2.id = QStringLiteral("p2");
        p2.groupId = g2.id;
        p2.name = QStringLiteral("P2");
        model.reset({g1, g2}, {p1, p2},
                    {makeTask(QStringLiteral("t1"), p1.id, Priority::High, 0,
                              {makeItem(QStringLiteral("i1")), makeItem(QStringLiteral("i2"))}),
                     makeTask(QStringLiteral("t2"), p1.id, Priority::Normal, 0, {makeItem(QStringLiteral("i3"))}),
                     makeTask(QStringLiteral("t3"), p1.id, Priority::Normal, 1, {makeItem(QStringLiteral("i4"))}),
                     makeTask(QStringLiteral("t4"), p1.id, Priority::Urgent, 0, {makeItem(QStringLiteral("i5"))},
                              TaskState::Done),
                     makeTask(QStringLiteral("t5"), p2.id, Priority::Normal, 0, {makeItem(QStringLiteral("i6"))})},
                    {});
        model.setWorkspaceId(QStringLiteral("ws"));

        secrets = std::make_unique<SecretStore>(makeMemoryBackend());
        servers = std::make_unique<ServerClients>(model, directory, *secrets);
        cache = std::make_unique<StatusCache>(model, directory, *servers, *device);
        attention = std::make_unique<Attention>(model, *cache, *device);
    }

    // Gives the item a status with these attention reasons, which never
    // clear on view, so each counts at once.
    void setReasons(const QString &itemId, const QStringList &keys)
    {
        ItemStatus status;
        status.state = ItemStatus::State::Ok;
        status.data.insert(QStringLiteral("id"), itemId);
        for (const QString &key : keys) {
            AttentionReason reason;
            reason.key = key;
            reason.text = key;
            status.attention << reason;
        }
        directory.type.next[itemId] = status;
        cache->refresh(itemId);
    }

    Directory directory;
    QTemporaryDir dir;
    std::unique_ptr<QSettings> settingsFile;
    std::unique_ptr<DeviceSettings> device;
    Model model;
    std::unique_ptr<SecretStore> secrets;
    std::unique_ptr<ServerClients> servers;
    std::unique_ptr<StatusCache> cache;
    std::unique_ptr<Attention> attention;
};

} // namespace mirante::attentiontest
