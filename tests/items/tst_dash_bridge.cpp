// Tests DashBridge on the workspace of the status tests: what each member
// returns, that it sees only its own task and group, the changed() signal,
// and that it opens only what it may see. See the M9 plan, part 3.
#include "../api/StatusTestSupport.h"

#include "api/Attention.h"
#include "items/DashBridge.h"
#include "items/ItemTypeRegistry.h"

#include <QApplication>
#include <QJsonArray>
#include <QSignalSpy>
#include <QWidget>

using namespace mirante;
using namespace mirante::statustest;

namespace {

// The registry's copy of the test module, whose status type answers from the
// fixture's controller.
Controller *registryController = nullptr;

void registerTest(ModuleRegistrar &registrar)
{
    registrar.addItemType(std::make_unique<StatusType>(*registryController));
    registrar.addItemType(std::make_unique<PlainType>());
}

QStringList idsOf(const QJsonArray &list)
{
    QStringList ids;
    for (const QJsonValue &value : list)
        ids << value.toObject().value(QStringLiteral("id")).toString();
    return ids;
}

} // namespace

class TestDashBridge : public QObject
{
    Q_OBJECT

    std::unique_ptr<Fixture> f;
    std::unique_ptr<ItemTypeRegistry> types;
    std::unique_ptr<Attention> attention;
    PlacementServices services;
    QStringList opened;

    void give(const QString &itemId, const ItemStatus &status)
    {
        f->controller.next[itemId] = status;
        f->cache->refresh(itemId);
    }

    std::unique_ptr<DashBridge> bridge(const QString &taskId = QStringLiteral("t1"))
    {
        return std::make_unique<DashBridge>(services, taskId);
    }

private slots:
    void init()
    {
        f = std::make_unique<Fixture>();
        registryController = &f->controller;
        types = std::make_unique<ItemTypeRegistry>();
        types->registerModule(QStringLiteral("test"), &registerTest);
        attention = std::make_unique<Attention>(f->model, *f->cache, *f->device);
        services = PlacementServices();
        services.model = &f->model;
        services.types = types.get();
        services.status = f->cache.get();
        services.attention = attention.get();
        opened.clear();
        services.openItem = [this](const QString &taskId, const QString &itemId) {
            opened << taskId + QLatin1Char('/') + itemId;
        };
        services.openTask = [this](const QString &taskId) { opened << taskId; };
    }

    void cleanup()
    {
        attention.reset();
        types.reset();
        f.reset();
    }

    void currentTask()
    {
        const QJsonObject task = bridge()->currentTask();
        QCOMPARE(task.value(QStringLiteral("id")).toString(), QStringLiteral("t1"));
        QCOMPARE(task.value(QStringLiteral("title")).toString(), QStringLiteral("t1"));
        QCOMPARE(task.value(QStringLiteral("project")).toString(), QStringLiteral("P1"));
        QCOMPARE(task.value(QStringLiteral("group")).toString(), QStringLiteral("G1"));
        QCOMPARE(task.value(QStringLiteral("groupId")).toString(), QStringLiteral("g1"));
        QCOMPARE(task.value(QStringLiteral("priority")).toInt(), int(Priority::Normal));
        QCOMPARE(task.value(QStringLiteral("priorityName")).toString(), QStringLiteral("Normal"));
        QCOMPARE(task.value(QStringLiteral("state")).toString(), QStringLiteral("active"));

        const QJsonArray items = task.value(QStringLiteral("items")).toArray();
        QCOMPARE(idsOf(items), QStringList({QStringLiteral("i1"), QStringLiteral("i2"), QStringLiteral("i3")}));
        QCOMPARE(items.at(0).toObject().value(QStringLiteral("hasStatus")).toBool(), true);
        QCOMPARE(items.at(0).toObject().value(QStringLiteral("typeName")).toString(), QStringLiteral("Status"));
        QCOMPARE(items.at(1).toObject().value(QStringLiteral("hasStatus")).toBool(), false);
        // A type of a module not in the build.
        QCOMPARE(items.at(2).toObject().value(QStringLiteral("hasStatus")).toBool(), false);
        QCOMPARE(items.at(2).toObject().value(QStringLiteral("typeName")).toString(), QString());
    }

    void goneTask()
    {
        QVERIFY(bridge(QStringLiteral("nope"))->currentTask().isEmpty());
        QVERIFY(bridge(QStringLiteral("nope"))->groupTasks(QStringLiteral("g1")).isEmpty());
    }

    void itemStatus()
    {
        auto panel = bridge();
        QJsonObject status = panel->itemStatus(QStringLiteral("i1"));
        QCOMPARE(status.value(QStringLiteral("state")).toString(), QStringLiteral("none"));
        QCOMPARE(status.value(QStringLiteral("updated")).toString(), QString());
        QVERIFY(status.value(QStringLiteral("closed")).isNull());

        // The first status records the key that clears on view; the failure counts.
        give(QStringLiteral("i1"), okStatus(QStringLiteral("Open"), {reason(QStringLiteral("pipeline-failed"))}));
        status = panel->itemStatus(QStringLiteral("i1"));
        QCOMPARE(status.value(QStringLiteral("state")).toString(), QStringLiteral("ok"));
        QCOMPARE(status.value(QStringLiteral("summary")).toString(), QStringLiteral("Open"));
        QVERIFY(!status.value(QStringLiteral("updated")).toString().isEmpty());
        QCOMPARE(status.value(QStringLiteral("needsAttention")).toBool(), true);
        QCOMPARE(status.value(QStringLiteral("attention")).toArray(), QJsonArray({QStringLiteral("pipeline-failed")}));
    }

    void itemStatusStates_data()
    {
        QTest::addColumn<int>("state");
        QTest::addColumn<QString>("name");
        QTest::newRow("no server") << int(ItemStatus::State::NoServer) << "noserver";
        QTest::newRow("no token") << int(ItemStatus::State::NoToken) << "notoken";
        QTest::newRow("refused") << int(ItemStatus::State::Refused) << "refused";
        QTest::newRow("not found") << int(ItemStatus::State::NotFound) << "notfound";
        QTest::newRow("error") << int(ItemStatus::State::Error) << "error";
    }

    void itemStatusStates()
    {
        QFETCH(int, state);
        QFETCH(QString, name);
        ItemStatus status;
        status.state = ItemStatus::State(state);
        status.message = QStringLiteral("Why");
        give(QStringLiteral("i1"), status);
        const QJsonObject json = bridge()->itemStatus(QStringLiteral("i1"));
        QCOMPARE(json.value(QStringLiteral("state")).toString(), name);
        QCOMPARE(json.value(QStringLiteral("message")).toString(), QStringLiteral("Why"));
    }

    void closedItem()
    {
        ItemStatus status = okStatus(QStringLiteral("Merged"));
        status.closed = true;
        give(QStringLiteral("i1"), status);
        QCOMPARE(bridge()->itemStatus(QStringLiteral("i1")).value(QStringLiteral("closed")).toBool(), true);
    }

    void itemOfAnotherTask()
    {
        give(QStringLiteral("i4"), okStatus(QStringLiteral("Open")));
        QVERIFY(bridge()->itemStatus(QStringLiteral("i4")).isEmpty());
        QVERIFY(bridge()->itemStatus(QString()).isEmpty());
    }

    void groupTasks()
    {
        give(QStringLiteral("i4"), okStatus(QStringLiteral("Open"), {reason(QStringLiteral("pipeline-failed"))}));
        // t2 High before t1 Normal; not t3 Done, t4 in the trash, or g2's tasks.
        const QJsonArray tasks = bridge()->groupTasks(QStringLiteral("g1"));
        QCOMPARE(idsOf(tasks), QStringList({QStringLiteral("t2"), QStringLiteral("t1")}));
        const QJsonObject t2 = tasks.at(0).toObject();
        QCOMPARE(t2.value(QStringLiteral("project")).toString(), QStringLiteral("P1"));
        QCOMPARE(t2.value(QStringLiteral("priorityName")).toString(), QStringLiteral("High"));
        QCOMPARE(t2.value(QStringLiteral("attention")).toInt(), 1);
        // Tests install no translator, so the plural is not resolved.
        QVERIFY(t2.value(QStringLiteral("attentionText")).toString().startsWith(QStringLiteral("1 item")));
        QCOMPARE(t2.value(QStringLiteral("current")).toBool(), false);
        QCOMPARE(tasks.at(1).toObject().value(QStringLiteral("attention")).toInt(), 0);
        QCOMPARE(tasks.at(1).toObject().value(QStringLiteral("attentionText")).toString(), QString());
        QCOMPARE(tasks.at(1).toObject().value(QStringLiteral("current")).toBool(), true);
    }

    void otherGroup()
    {
        QVERIFY(bridge()->groupTasks(QStringLiteral("g2")).isEmpty());
        QVERIFY(bridge()->groupTasks(QString()).isEmpty());
    }

    void opensOnlyWhatItSees()
    {
        auto panel = bridge();
        panel->openItem(QStringLiteral("i2"));
        panel->openItem(QStringLiteral("i4"));      // another task's item
        panel->openTask(QStringLiteral("t2"));
        panel->openTask(QStringLiteral("t3"));      // Done, still in the group
        panel->openTask(QStringLiteral("t4"));      // in the trash
        panel->openTask(QStringLiteral("t5"));      // another group
        QCOMPARE(opened, QStringList({QStringLiteral("t1/i2"), QStringLiteral("t2"), QStringLiteral("t3")}));
    }

    void changedOncePerPass()
    {
        auto panel = bridge();
        QSignalSpy changed(panel.get(), &DashBridge::changed);
        give(QStringLiteral("i1"), okStatus(QStringLiteral("Open")));
        give(QStringLiteral("i1"), okStatus(QStringLiteral("Merged")));
        Task t2 = *f->model.task(QStringLiteral("t2"));
        t2.title = QStringLiteral("Renamed");
        f->model.setTask(t2);
        QCOMPARE(changed.count(), 0);
        QVERIFY(changed.wait(1000));
        QTest::qWait(50);
        QCOMPARE(changed.count(), 1);
    }

    void notChangedByOtherGroups()
    {
        auto panel = bridge();
        QSignalSpy changed(panel.get(), &DashBridge::changed);
        Task t5 = *f->model.task(QStringLiteral("t5"));
        t5.title = QStringLiteral("Renamed");
        f->model.setTask(t5);
        // i7 is t5's item. Its reason changes the workspace's counts, but not
        // those the panel shows.
        f->controller.next[QStringLiteral("i7")] =
            okStatus(QStringLiteral("Open"), {reason(QStringLiteral("pipeline-failed"))});
        f->cache->refresh(QStringLiteral("i7"));
        QTest::qWait(50);
        QCOMPARE(changed.count(), 0);
    }

    void changedByPalette()
    {
        auto panel = bridge();
        QWidget widget;
        panel->followPalette(&widget);
        QSignalSpy changed(panel.get(), &DashBridge::changed);
        QPalette palette = widget.palette();
        palette.setColor(QPalette::Window, Qt::black);
        widget.setPalette(palette);
        QVERIFY(changed.wait(1000));
    }

    // Every key that the panels use. resources/panels/ reads these.
    void texts()
    {
        const QJsonObject texts = bridge()->texts();
        for (const char *key : {"noStatusItems", "noActiveTasks", "notFetched", "closed", "thisTask", "taskGone"})
            QVERIFY2(!texts.value(QLatin1String(key)).toString().isEmpty(), key);
    }

    void palette()
    {
        QPalette app = QApplication::palette();
        const QPalette saved = app;
        app.setColor(QPalette::Window, QColor(0x12, 0x34, 0x56));
        QApplication::setPalette(app);
        const QJsonObject colours = bridge()->palette();
        QApplication::setPalette(saved);

        QCOMPARE(colours.value(QStringLiteral("window")).toString(), QStringLiteral("#123456"));
        QCOMPARE(colours.value(QStringLiteral("priorityUrgent")).toString(), QStringLiteral("#d93b3b"));
        QVERIFY(colours.value(QStringLiteral("priorityNormal")).toString().startsWith(QStringLiteral("rgba(")));
        for (const char *key : {"base", "text", "muted", "border", "accent", "link", "attention"})
            QVERIFY2(!colours.value(QLatin1String(key)).toString().isEmpty(), key);
        QVERIFY(colours.value(QStringLiteral("fontSize")).toInt() > 0);
    }
};

QTEST_MAIN(TestDashBridge)
#include "tst_dash_bridge.moc"
