// Tests connections::resolve: the order of item, project and group, the order
// of kinds, the wrong kind, a connection of another group, and a missing
// project or task.
#include "core/Connections.h"
#include "core/Entities.h"
#include "core/Model.h"

#include <QTest>

using namespace mirante;

namespace {

Connection makeConnection(const QString &id, const QString &kind)
{
    Connection connection;
    connection.id = id;
    connection.kind = kind;
    connection.name = id;
    connection.baseUrl = QStringLiteral("https://%1.example.com").arg(id);
    return connection;
}

Item makeItem(const QString &connectionId = QString())
{
    Item item;
    item.id = QStringLiteral("i1");
    item.type = QStringLiteral("jira.issue.page");
    item.connectionId = connectionId;
    return item;
}

const QString cloud = QStringLiteral("jira.cloud");
const QString dc = QStringLiteral("jira.dc");
const QString gitlab = QStringLiteral("gitlab");

} // namespace

class TestConnections : public QObject
{
    Q_OBJECT

private:
    Group g1;
    Group g2;
    Project p1;
    Task t1;

    // g1 holds p1, which holds t1. g1 has cloud1, cloud2, dc1 and gitlab1.
    // g2 has other1, of kind jira.cloud. Each test changes these, then calls
    // load().
    void load(Model &model) const { model.reset({g1, g2}, {p1}, {t1}, {}); }

    // The id of the resolved connection, or an empty string.
    QString resolvedId(const Item &item, const QStringList &kinds) const
    {
        Model model;
        load(model);
        const Connection *connection = connections::resolve(model, t1.id, item, kinds);
        return connection ? connection->id : QString();
    }

    bool resolves(const Item &item, const QStringList &kinds) const
    {
        Model model;
        load(model);
        return connections::resolve(model, t1.id, item, kinds) != nullptr;
    }

private slots:
    void init()
    {
        g1 = Group();
        g1.id = QStringLiteral("g1");
        g1.connections = {makeConnection(QStringLiteral("cloud1"), cloud),
                          makeConnection(QStringLiteral("cloud2"), cloud), makeConnection(QStringLiteral("dc1"), dc),
                          makeConnection(QStringLiteral("gitlab1"), gitlab)};
        g2 = Group();
        g2.id = QStringLiteral("g2");
        g2.connections = {makeConnection(QStringLiteral("other1"), cloud)};
        p1 = Project();
        p1.id = QStringLiteral("p1");
        p1.groupId = g1.id;
        t1 = Task();
        t1.id = QStringLiteral("t1");
        t1.projectId = p1.id;
    }

    void nothingSet()
    {
        QCOMPARE(resolvedId(makeItem(), {cloud, dc}), QString());
    }

    void groupDefault()
    {
        g1.defaultConnections.insert(cloud, QStringLiteral("cloud2"));
        QCOMPARE(resolvedId(makeItem(), {cloud}), QStringLiteral("cloud2"));
    }

    void returnsTheModelsConnection()
    {
        g1.defaultConnections.insert(cloud, QStringLiteral("cloud2"));
        Model model;
        load(model);
        const Connection *connection = connections::resolve(model, t1.id, makeItem(), {cloud});
        QVERIFY(connection);
        QCOMPARE(connection, &model.group(g1.id)->connections[1]);
        QCOMPARE(connection->baseUrl, QStringLiteral("https://cloud2.example.com"));
    }

    void projectBeforeGroup()
    {
        g1.defaultConnections.insert(cloud, QStringLiteral("cloud1"));
        p1.connectionOverrides.insert(cloud, QStringLiteral("cloud2"));
        QCOMPARE(resolvedId(makeItem(), {cloud}), QStringLiteral("cloud2"));
    }

    void itemBeforeProject()
    {
        g1.defaultConnections.insert(cloud, QStringLiteral("cloud1"));
        p1.connectionOverrides.insert(cloud, QStringLiteral("cloud1"));
        QCOMPARE(resolvedId(makeItem(QStringLiteral("cloud2")), {cloud}), QStringLiteral("cloud2"));
    }

    void itemOfAnyListedKind()
    {
        g1.defaultConnections.insert(cloud, QStringLiteral("cloud1"));
        // The item's choice wins even when its kind comes second.
        QCOMPARE(resolvedId(makeItem(QStringLiteral("dc1")), {cloud, dc}), QStringLiteral("dc1"));
    }

    void kindsInOrder()
    {
        g1.defaultConnections.insert(cloud, QStringLiteral("cloud1"));
        g1.defaultConnections.insert(dc, QStringLiteral("dc1"));
        QCOMPARE(resolvedId(makeItem(), {cloud, dc}), QStringLiteral("cloud1"));
        QCOMPARE(resolvedId(makeItem(), {dc, cloud}), QStringLiteral("dc1"));

        p1.connectionOverrides.insert(cloud, QStringLiteral("cloud2"));
        p1.connectionOverrides.insert(dc, QStringLiteral("dc1"));
        QCOMPARE(resolvedId(makeItem(), {cloud, dc}), QStringLiteral("cloud2"));
        QCOMPARE(resolvedId(makeItem(), {dc, cloud}), QStringLiteral("dc1"));
    }

    void stepBeforeKind()
    {
        // A project override of the second kind wins over a group default of
        // the first, because the steps come before the kinds.
        g1.defaultConnections.insert(cloud, QStringLiteral("cloud1"));
        p1.connectionOverrides.insert(dc, QStringLiteral("dc1"));
        QCOMPARE(resolvedId(makeItem(), {cloud, dc}), QStringLiteral("dc1"));
    }

    void laterKindWhenFirstMissing()
    {
        g1.defaultConnections.insert(dc, QStringLiteral("dc1"));
        QCOMPARE(resolvedId(makeItem(), {cloud, dc}), QStringLiteral("dc1"));
    }

    void wrongKind()
    {
        // The item names a GitLab connection, so the step is skipped.
        g1.defaultConnections.insert(cloud, QStringLiteral("cloud1"));
        QCOMPARE(resolvedId(makeItem(QStringLiteral("gitlab1")), {cloud}), QStringLiteral("cloud1"));

        // A project override and a default that point to another kind count
        // for nothing.
        p1.connectionOverrides.insert(cloud, QStringLiteral("dc1"));
        g1.defaultConnections.insert(cloud, QStringLiteral("gitlab1"));
        QCOMPARE(resolvedId(makeItem(QStringLiteral("gitlab1")), {cloud}), QString());

        // A kind the item does not list is never used.
        g1.defaultConnections.insert(gitlab, QStringLiteral("gitlab1"));
        QCOMPARE(resolvedId(makeItem(), {cloud, dc}), QString());
    }

    void otherGroup()
    {
        g1.defaultConnections.insert(cloud, QStringLiteral("cloud1"));
        QCOMPARE(resolvedId(makeItem(QStringLiteral("other1")), {cloud}), QStringLiteral("cloud1"));

        p1.connectionOverrides.insert(cloud, QStringLiteral("other1"));
        QCOMPARE(resolvedId(makeItem(), {cloud}), QStringLiteral("cloud1"));

        g1.defaultConnections.insert(cloud, QStringLiteral("other1"));
        QCOMPARE(resolvedId(makeItem(QStringLiteral("other1")), {cloud}), QString());
    }

    void unknownId()
    {
        g1.defaultConnections.insert(cloud, QStringLiteral("cloud1"));
        p1.connectionOverrides.insert(cloud, QStringLiteral("gone"));
        QCOMPARE(resolvedId(makeItem(QStringLiteral("gone")), {cloud}), QStringLiteral("cloud1"));
    }

    void noKinds()
    {
        g1.defaultConnections.insert(cloud, QStringLiteral("cloud1"));
        QCOMPARE(resolvedId(makeItem(QStringLiteral("cloud1")), {}), QString());
    }

    void missingTask()
    {
        g1.defaultConnections.insert(cloud, QStringLiteral("cloud1"));
        Model model;
        load(model);
        QVERIFY(!connections::resolve(model, QStringLiteral("nope"), makeItem(QStringLiteral("cloud1")), {cloud}));
        // The same item resolves through a task that exists.
        QVERIFY(connections::resolve(model, t1.id, makeItem(QStringLiteral("cloud1")), {cloud}));
    }

    void missingProject()
    {
        g1.defaultConnections.insert(cloud, QStringLiteral("cloud1"));
        t1.projectId = QStringLiteral("nope");
        QVERIFY(!resolves(makeItem(QStringLiteral("cloud1")), {cloud}));
    }

    void missingGroup()
    {
        g1.defaultConnections.insert(cloud, QStringLiteral("cloud1"));
        p1.groupId = QStringLiteral("nope");
        QVERIFY(!resolves(makeItem(QStringLiteral("cloud1")), {cloud}));
    }
};

QTEST_MAIN(TestConnections)
#include "tst_connections.moc"
