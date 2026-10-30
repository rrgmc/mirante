#pragma once

// What tst_status_cache and tst_attention share: a module directory with one
// item type that reports status, a workspace that uses it, and the services a
// StatusCache needs, with a scratch settings file and a clock the test moves.

#include "api/ModuleDirectory.h"
#include "api/SecretBackend.h"
#include "api/SecretStore.h"
#include "api/ServerClient.h"
#include "api/StatusCache.h"
#include "core/DeviceSettings.h"
#include "core/Model.h"

#include <mirante/ItemType.h>

#include <QJsonObject>
#include <QList>
#include <QPointer>
#include <QSettings>
#include <QStringList>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QTest>

#include <map>
#include <memory>

namespace mirante::statustest {

// How the test type's fetchStatus() ends.
enum class Fetch {
    Finish,     // at once, with Controller::next
    Hold,       // not yet: the job waits in Controller::held
    Server,     // with what one GET through the job's server answers
};

struct Controller
{
    Fetch mode = Fetch::Finish;
    // The status each item finishes with, by item id; ItemStatus() if none.
    std::map<QString, ItemStatus> next;
    // The item ids fetched, in order.
    QStringList fetched;
    // The jobs held, with their item ids.
    QList<QPair<QString, StatusJob *>> held;
};

// A type that reports status. Each item's configuration holds its own id,
// as "name", so the type can tell the items apart.
class StatusType : public ItemType
{
public:
    explicit StatusType(Controller &controller)
        : controller(controller)
    {
    }

    QString id() const override { return QStringLiteral("test.status"); }
    QString name() const override { return QStringLiteral("Status"); }
    QIcon icon() const override { return {}; }
    bool hasStatus() const override { return true; }

    void fetchStatus(StatusJob &job) const override
    {
        const QString itemId = job.config().value(QStringLiteral("name")).toString();
        controller.fetched << itemId;
        switch (controller.mode) {
        case Fetch::Finish:
            job.finish(controller.next[itemId]);
            break;
        case Fetch::Hold:
            controller.held << qMakePair(itemId, &job);
            break;
        case Fetch::Server:
            job.server().get(QStringLiteral("api/thing"), {}, {}, job.owner(),
                             [&job](const ApiReply &reply) { job.finish(statusFromReply(reply)); });
            break;
        }
    }

    Controller &controller;
};

// A type that reports no status.
class PlainType : public ItemType
{
public:
    QString id() const override { return QStringLiteral("test.plain"); }
    QString name() const override { return QStringLiteral("Plain"); }
    QIcon icon() const override { return {}; }
};

// The module "test", with the two types and no server type. Types of other
// modules, such as "missing.type", are not found, as for a module not in the
// build.
class TestDirectory : public ModuleDirectory
{
public:
    explicit TestDirectory(Controller &controller)
        : status(controller)
    {
    }

    QStringList moduleIds() const override { return {QStringLiteral("test")}; }
    const ItemType *findType(const QString &typeId) const override
    {
        if (typeId == status.id())
            return &status;
        if (typeId == plain.id())
            return &plain;
        return nullptr;
    }
    const ServerType *serverType(const QString &) const override { return nullptr; }
    const ModuleSettings *moduleSettings(const QString &) const override { return nullptr; }
    std::vector<const SettingsScreen *> settingsScreens(const QString &) const override { return {}; }

    StatusType status;
    PlainType plain;
};

inline Item makeItem(const QString &id, const QString &type = QStringLiteral("test.status"))
{
    Item item;
    item.id = id;
    item.type = type;
    item.name = id;
    item.config.insert(QStringLiteral("name"), id);
    return item;
}

inline Task makeTask(const QString &id, const QString &projectId, Priority priority, TaskState state,
                     std::vector<Item> items)
{
    Task task;
    task.id = id;
    task.projectId = projectId;
    task.title = id;
    task.priority = priority;
    task.state = state;
    task.items = std::move(items);
    Dashboard dashboard;
    dashboard.id = id + QStringLiteral("-d");
    task.dashboards.push_back(dashboard);
    task.defaultDashboardId = dashboard.id;
    return task;
}

// The workspace "ws1":
//   group g1: project p1, with
//     t1 Normal Active: i1 (status), i2 (plain), i3 (missing module)
//     t2 High Active:   i4
//     t3 Urgent Done:   i5
//     t4 High Active, in the trash: i6
//   group g2: project p2, with
//     t5 Urgent Active: i7
//     t6 Normal Archived: i8
inline void fillModel(Model &model)
{
    Group g1;
    g1.id = QStringLiteral("g1");
    g1.name = QStringLiteral("G1");
    Group g2;
    g2.id = QStringLiteral("g2");
    g2.name = QStringLiteral("G2");
    Project p1;
    p1.id = QStringLiteral("p1");
    p1.groupId = g1.id;
    p1.name = QStringLiteral("P1");
    Project p2;
    p2.id = QStringLiteral("p2");
    p2.groupId = g2.id;
    p2.name = QStringLiteral("P2");

    Task t4 = makeTask(QStringLiteral("t4"), p1.id, Priority::High, TaskState::Active, {makeItem(QStringLiteral("i6"))});
    t4.trashedAt = QStringLiteral("2026-10-01T00:00:00Z");

    model.reset({g1, g2}, {p1, p2},
                {makeTask(QStringLiteral("t1"), p1.id, Priority::Normal, TaskState::Active,
                          {makeItem(QStringLiteral("i1")), makeItem(QStringLiteral("i2"), QStringLiteral("test.plain")),
                           makeItem(QStringLiteral("i3"), QStringLiteral("missing.type"))}),
                 makeTask(QStringLiteral("t2"), p1.id, Priority::High, TaskState::Active, {makeItem(QStringLiteral("i4"))}),
                 makeTask(QStringLiteral("t3"), p1.id, Priority::Urgent, TaskState::Done, {makeItem(QStringLiteral("i5"))}),
                 t4,
                 makeTask(QStringLiteral("t5"), p2.id, Priority::Urgent, TaskState::Active, {makeItem(QStringLiteral("i7"))}),
                 makeTask(QStringLiteral("t6"), p2.id, Priority::Normal, TaskState::Archived,
                          {makeItem(QStringLiteral("i8"))})},
                {});
    model.setWorkspaceId(QStringLiteral("ws1"));
}

inline ItemStatus okStatus(const QString &summary, QList<AttentionReason> attention = {})
{
    ItemStatus status;
    status.state = ItemStatus::State::Ok;
    status.summary = summary;
    status.data.insert(QStringLiteral("summary"), summary);
    status.attention = std::move(attention);
    return status;
}

inline AttentionReason reason(const QString &key, bool clearsOnView = false)
{
    AttentionReason r;
    r.key = key;
    r.text = key;
    r.clearsOnView = clearsOnView;
    return r;
}

// Everything a StatusCache needs, made fresh for each test.
struct Fixture
{
    Fixture()
        : directory(controller)
    {
        QVERIFY(dir.isValid());
        settingsFile = std::make_unique<QSettings>(dir.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
        device = std::make_unique<DeviceSettings>(settingsFile.get());
        fillModel(model);
        secrets = std::make_unique<SecretStore>(makeMemoryBackend());
        servers = std::make_unique<ServerClients>(model, directory, *secrets);
        cache = std::make_unique<StatusCache>(model, directory, *servers, *device);
        now = QDateTime(QDate(2026, 10, 29), QTime(9, 0), QTimeZone::UTC);
        cache->setClock([this] { return now; });
    }

    void advance(int seconds) { now = now.addSecs(seconds); }

    Controller controller;
    TestDirectory directory;
    QTemporaryDir dir;
    std::unique_ptr<QSettings> settingsFile;
    std::unique_ptr<DeviceSettings> device;
    Model model;
    std::unique_ptr<SecretStore> secrets;
    std::unique_ptr<ServerClients> servers;
    std::unique_ptr<StatusCache> cache;
    QDateTime now;
};

} // namespace mirante::statustest
