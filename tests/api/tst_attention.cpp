// Tests Attention on a StatusCache fed by a test item type: the first look
// records the keys that clear on view, a new key counts until seen, states
// never clear, and the sums for tasks, projects and groups leave out tasks
// that are not active and tasks in the trash.
#include "StatusTestSupport.h"

#include "api/Attention.h"

#include <QSignalSpy>

using namespace mirante;
using namespace mirante::statustest;

class TestAttention : public QObject
{
    Q_OBJECT

    std::unique_ptr<Fixture> f;
    std::unique_ptr<Attention> attention;

    const QString ws = QStringLiteral("ws1");

    // Fetches the item with the given status.
    void give(const QString &itemId, const ItemStatus &status)
    {
        f->controller.next[itemId] = status;
        f->cache->refresh(itemId);
    }

    static QStringList keysOf(const QList<AttentionReason> &reasons)
    {
        QStringList keys;
        for (const AttentionReason &r : reasons)
            keys << r.key;
        return keys;
    }

private slots:
    void init()
    {
        f = std::make_unique<Fixture>();
        attention = std::make_unique<Attention>(f->model, *f->cache, *f->device);
    }

    void cleanup()
    {
        attention.reset();
        f.reset();
    }

    void firstLookRecordsKeys()
    {
        const QString i1 = QStringLiteral("i1");
        give(i1, okStatus(QStringLiteral("In Review"), {reason(QStringLiteral("status:3"), true)}));
        QVERIFY(!attention->needsAttention(i1));
        QVERIFY(attention->reasons(i1).isEmpty());
        QVERIFY(f->device->hasSeenAttention(ws, i1));
        QCOMPARE(f->device->seenAttention(ws, i1), QStringList({QStringLiteral("status:3")}));
    }

    // A first status without data, such as a missing token, records
    // nothing, so the first real status still counts as the first look.
    void firstLookNeedsData()
    {
        const QString i1 = QStringLiteral("i1");
        ItemStatus noToken;
        noToken.state = ItemStatus::State::NoToken;
        give(i1, noToken);
        QVERIFY(!f->device->hasSeenAttention(ws, i1));
        attention->markSeen(i1);
        QVERIFY(!f->device->hasSeenAttention(ws, i1));

        give(i1, okStatus(QStringLiteral("In Review"), {reason(QStringLiteral("status:3"), true)}));
        QVERIFY(!attention->needsAttention(i1));
        QCOMPARE(f->device->seenAttention(ws, i1), QStringList({QStringLiteral("status:3")}));
    }

    void newKeyCountsUntilSeen()
    {
        const QString i1 = QStringLiteral("i1");
        give(i1, okStatus(QStringLiteral("To Do"), {reason(QStringLiteral("status:1"), true)}));
        QVERIFY(!attention->needsAttention(i1));

        give(i1, okStatus(QStringLiteral("In Review"), {reason(QStringLiteral("status:3"), true)}));
        QVERIFY(attention->needsAttention(i1));
        QCOMPARE(keysOf(attention->reasons(i1)), QStringList({QStringLiteral("status:3")}));

        attention->markSeen(i1);
        QVERIFY(!attention->needsAttention(i1));
        // Keys no longer reported drop out of the record.
        QCOMPARE(f->device->seenAttention(ws, i1), QStringList({QStringLiteral("status:3")}));

        // Back to a status seen long ago: it counts again, as its key left the record.
        give(i1, okStatus(QStringLiteral("To Do"), {reason(QStringLiteral("status:1"), true)}));
        QVERIFY(attention->needsAttention(i1));
    }

    // An event that appears later on an item with a record counts.
    void laterEventCounts()
    {
        const QString i1 = QStringLiteral("i1");
        give(i1, okStatus(QStringLiteral("Open")));
        QVERIFY(f->device->hasSeenAttention(ws, i1));
        give(i1, okStatus(QStringLiteral("Open"), {reason(QStringLiteral("approved"), true)}));
        QCOMPARE(keysOf(attention->reasons(i1)), QStringList({QStringLiteral("approved")}));
    }

    void statesNeverClear()
    {
        const QString i1 = QStringLiteral("i1");
        give(i1, okStatus(QStringLiteral("Open"), {reason(QStringLiteral("pipeline-failed")),
                                                   reason(QStringLiteral("approved"), true)}));
        QCOMPARE(keysOf(attention->reasons(i1)), QStringList({QStringLiteral("pipeline-failed")}));
        attention->markSeen(i1);
        QCOMPARE(keysOf(attention->reasons(i1)), QStringList({QStringLiteral("pipeline-failed")}));
        QVERIFY(attention->needsAttention(i1));
    }

    // Without a workspace id there is nowhere to keep seen keys: events do
    // not count, states do.
    void noWorkspaceId()
    {
        f->model.setWorkspaceId(QString());
        const QString i1 = QStringLiteral("i1");
        give(i1, okStatus(QStringLiteral("Open"), {reason(QStringLiteral("pipeline-failed")),
                                                   reason(QStringLiteral("approved"), true)}));
        give(i1, okStatus(QStringLiteral("Open"), {reason(QStringLiteral("pipeline-failed")),
                                                   reason(QStringLiteral("status:2"), true)}));
        QCOMPARE(keysOf(attention->reasons(i1)), QStringList({QStringLiteral("pipeline-failed")}));
        attention->markSeen(i1);
        QVERIFY(!f->device->hasSeenAttention(QString(), i1));
    }

    void sums()
    {
        const ItemStatus failed = okStatus(QStringLiteral("Open"), {reason(QStringLiteral("pipeline-failed"))});
        // t1 Normal Active, t2 High Active, t3 Done, t5 in the other group,
        // t6 Archived.
        for (const QString &itemId : {QStringLiteral("i1"), QStringLiteral("i4"), QStringLiteral("i5"),
                                      QStringLiteral("i7"), QStringLiteral("i8")})
            give(itemId, failed);
        // i6 is in the trash, so the cache never fetches it.

        QCOMPARE(attention->taskCount(QStringLiteral("t1")), 1);
        QCOMPARE(attention->taskCount(QStringLiteral("t2")), 1);
        QCOMPARE(attention->taskCount(QStringLiteral("t3")), 0);
        QCOMPARE(attention->taskCount(QStringLiteral("t4")), 0);
        QCOMPARE(attention->taskCount(QStringLiteral("t6")), 0);
        QCOMPARE(attention->taskCount(QStringLiteral("unknown")), 0);
        QCOMPARE(attention->projectCount(QStringLiteral("p1")), 2);
        QCOMPARE(attention->projectCount(QStringLiteral("p2")), 1);
        QCOMPARE(attention->groupCount(QStringLiteral("g1")), 2);
        QCOMPARE(attention->groupCount(QStringLiteral("g2")), 1);

        // A task moved to the trash keeps its status but counts zero.
        Task t2 = *f->model.task(QStringLiteral("t2"));
        t2.trashedAt = QStringLiteral("2026-10-29T09:00:00Z");
        f->model.setTask(t2);
        QVERIFY(attention->needsAttention(QStringLiteral("i4")));
        QCOMPARE(attention->taskCount(QStringLiteral("t2")), 0);
        QCOMPARE(attention->projectCount(QStringLiteral("p1")), 1);

        // A project in the trash counts zero.
        Project p1 = *f->model.project(QStringLiteral("p1"));
        p1.trashedAt = QStringLiteral("2026-10-29T09:00:00Z");
        f->model.setProject(p1);
        QCOMPARE(attention->projectCount(QStringLiteral("p1")), 0);
        QCOMPARE(attention->groupCount(QStringLiteral("g1")), 0);
    }

    // The tray counts active tasks with a watched item that needs attention:
    // a task with two such items once, an unwatched item never.
    void trayCount()
    {
        Task t1 = *f->model.task(QStringLiteral("t1"));
        t1.items.push_back(makeItem(QStringLiteral("i9")));
        f->model.setTask(t1);
        const auto watch = [this](const QString &taskId, const QString &itemId) {
            Item item = *f->model.item(taskId, itemId);
            item.watch = true;
            f->model.setItem(taskId, item);
        };
        watch(QStringLiteral("t1"), QStringLiteral("i1"));
        watch(QStringLiteral("t1"), QStringLiteral("i9"));
        watch(QStringLiteral("t3"), QStringLiteral("i5"));

        const ItemStatus failed = okStatus(QStringLiteral("Open"), {reason(QStringLiteral("pipeline-failed"))});
        for (const QString &itemId : {QStringLiteral("i1"), QStringLiteral("i9"), QStringLiteral("i4"),
                                      QStringLiteral("i5")})
            give(itemId, failed);
        // i4 needs attention but is not watched; t3 is done.
        QCOMPARE(attention->trayCount(), 1);

        watch(QStringLiteral("t2"), QStringLiteral("i4"));
        QCOMPARE(attention->trayCount(), 2);

        // Watching is a change of the task, so the counts change.
        QSignalSpy counts(attention.get(), &Attention::countsChanged);
        Item i4 = *f->model.item(QStringLiteral("t2"), QStringLiteral("i4"));
        i4.watch = false;
        f->model.setItem(QStringLiteral("t2"), i4);
        QTRY_COMPARE(counts.size(), 1);
        QCOMPARE(attention->trayCount(), 1);
    }

    void countsChangedOncePerPass()
    {
        QSignalSpy counts(attention.get(), &Attention::countsChanged);
        give(QStringLiteral("i1"), okStatus(QStringLiteral("a")));
        give(QStringLiteral("i4"), okStatus(QStringLiteral("b")));
        QCOMPARE(counts.size(), 0);
        QTRY_COMPARE(counts.size(), 1);
        QTest::qWait(10);
        QCOMPARE(counts.size(), 1);

        // A task's state changes what it counts.
        Task t1 = *f->model.task(QStringLiteral("t1"));
        t1.state = TaskState::Done;
        f->model.setTask(t1);
        QTRY_COMPARE(counts.size(), 2);

        // Seeing an event changes the counts; seeing it again does not.
        give(QStringLiteral("i4"), okStatus(QStringLiteral("b"), {reason(QStringLiteral("approved"), true)}));
        QTRY_COMPARE(counts.size(), 3);
        attention->markSeen(QStringLiteral("i4"));
        QTRY_COMPARE(counts.size(), 4);
        attention->markSeen(QStringLiteral("i4"));
        QTest::qWait(10);
        QCOMPARE(counts.size(), 4);
    }
};

QTEST_GUILESS_MAIN(TestAttention)
#include "tst_attention.moc"
