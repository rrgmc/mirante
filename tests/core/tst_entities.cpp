// fromJson and toJson of every entity: defaults, unknown and wrong-type
// fields kept, unusable data reported, and round trips.
#include "core/Entities.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QTest>

using namespace mirante;

// URLs in the JSON below write "/" as "\/", because moc reads "//" in a
// multi-line raw string as the start of a comment.

namespace {

QJsonObject parse(const char *text)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(QByteArray(text), &error);
    if (error.error != QJsonParseError::NoError)
        qFatal("bad test JSON: %s", qPrintable(error.errorString()));
    return document.object();
}

template <typename T>
QJsonObject roundTrip(const QJsonObject &json)
{
    QString error;
    const auto entity = T::fromJson(json, &error);
    if (!entity) {
        qWarning("fromJson failed: %s", qPrintable(error));
        return {};
    }
    return entity->toJson();
}

const char *const FullTask = R"({
    "id": "t-1", "projectId": "p-1", "title": "Fix login",
    "isDefault": false, "state": "done", "priority": 1, "order": 4,
    "items": [
        { "id": "i-1", "type": "gitlab.mr.summary", "name": "MR",
          "config": { "mr": 12, "nested": { "a": [1, 2] } }, "connectionId": "c-2" },
        { "id": "i-2", "type": "timetrack.entry", "name": "Hours", "config": {},
          "futureField": true }
    ],
    "dashboards": [
        { "id": "d-1", "name": "Main", "layout": {
            "type": "split", "dir": "h", "sizes": [2, 1],
            "children": [
                { "type": "cell", "ref": { "item": "i-1" } },
                { "type": "tabs", "current": 1, "collapseSingle": true, "tabs": [
                    { "title": "A", "node": { "type": "cell", "ref": null } },
                    { "title": "B", "node": { "type": "split", "dir": "v", "sizes": [0.5, 1.5],
                      "children": [
                        { "type": "cell", "ref": { "item": "i-2" } },
                        { "type": "cell", "ref": null, "pinned": "yes" } ] } }
                ] }
            ] } },
        { "id": "d-2", "name": "Other", "layout": { "type": "cell", "ref": null } }
    ],
    "defaultDashboardId": "d-1",
    "unknownTop": { "keep": "me" }
})";

} // namespace

class TestEntities : public QObject
{
    Q_OBJECT

private slots:
    // Folder

    void folderDefaults()
    {
        const auto folder = Folder::fromJson(parse(R"({ "id": "f-1" })"));
        QVERIFY(folder);
        QCOMPARE(folder->id, QStringLiteral("f-1"));
        QVERIFY(folder->name.isEmpty());
        QVERIFY(folder->remoteUrl.isEmpty());
        QVERIFY(!folder->main);
        QVERIFY(folder->extra.isEmpty());
        // remoteUrl is omitted when empty.
        QCOMPARE(folder->toJson(), parse(R"({ "id": "f-1", "name": "", "main": false })"));
    }

    void folderFields()
    {
        const auto folder = Folder::fromJson(parse(
            R"({ "id": "f-1", "name": "backend", "remoteUrl": "git@x:a/b.git", "main": true })"));
        QVERIFY(folder);
        QCOMPARE(folder->name, QStringLiteral("backend"));
        QCOMPARE(folder->remoteUrl, QStringLiteral("git@x:a/b.git"));
        QVERIFY(folder->main);
    }

    // Connection

    void connectionDefaults()
    {
        const auto connection = Connection::fromJson(parse(R"({ "id": "c-1" })"));
        QVERIFY(connection);
        QCOMPARE(connection->toJson(), parse(
            R"({ "id": "c-1", "kind": "", "name": "", "baseUrl": "", "fields": {} })"));
    }

    void connectionFields()
    {
        const auto connection = Connection::fromJson(parse(R"({ "id": "c-1", "kind": "jira.cloud",
            "name": "Acme", "baseUrl": "https:\/\/acme.atlassian.net", "fields": { "email": "a@b.c" } })"));
        QVERIFY(connection);
        QCOMPARE(connection->kind, QStringLiteral("jira.cloud"));
        QCOMPARE(connection->name, QStringLiteral("Acme"));
        QCOMPARE(connection->baseUrl, QStringLiteral("https://acme.atlassian.net"));
        QCOMPARE(connection->fields.value(QStringLiteral("email")).toString(), QStringLiteral("a@b.c"));
    }

    // Group

    void groupDefaults()
    {
        const auto group = Group::fromJson(parse(R"({ "id": "g-1" })"));
        QVERIFY(group);
        QCOMPARE(group->order, 0);
        QVERIFY(group->connections.empty());
        QCOMPARE(group->toJson(), parse(R"({ "id": "g-1", "name": "", "order": 0, "connections": [],
            "defaultConnections": {}, "profileOverrides": {} })"));
    }

    void groupFields()
    {
        const auto group = Group::fromJson(parse(R"({ "id": "g-1", "name": "Work", "order": 2,
            "connections": [ { "id": "c-1", "kind": "gitlab" }, { "id": "c-2", "kind": "timetrack" } ],
            "defaultConnections": { "gitlab": "c-1" },
            "profileOverrides": { "github.com": "shared" } })"));
        QVERIFY(group);
        QCOMPARE(group->name, QStringLiteral("Work"));
        QCOMPARE(group->order, 2);
        QCOMPARE(group->connections.size(), std::size_t(2));
        QCOMPARE(group->connections[1].kind, QStringLiteral("timetrack"));
        QCOMPARE(group->defaultConnections.value(QStringLiteral("gitlab")), QStringLiteral("c-1"));
        QCOMPARE(group->profileOverrides.value(QStringLiteral("github.com")).toString(),
                 QStringLiteral("shared"));
    }

    // Project

    void projectDefaults()
    {
        const auto project = Project::fromJson(parse(R"({ "id": "p-1", "groupId": "g-1" })"));
        QVERIFY(project);
        QCOMPARE(project->priority, Priority::Normal);
        QCOMPARE(project->toJson(), parse(R"({ "id": "p-1", "groupId": "g-1", "name": "",
            "priority": 2, "order": 0, "folders": [], "moduleSettings": {},
            "connectionOverrides": {} })"));
    }

    void projectFields()
    {
        const auto project = Project::fromJson(parse(R"({ "id": "p-1", "groupId": "g-1",
            "name": "Backend", "priority": 0, "order": 3,
            "folders": [ { "id": "f-1", "main": true } ],
            "moduleSettings": { "timetrack": { "code": "X" } },
            "connectionOverrides": { "jira.cloud": "c-9" } })"));
        QVERIFY(project);
        QCOMPARE(project->groupId, QStringLiteral("g-1"));
        QCOMPARE(project->priority, Priority::Urgent);
        QCOMPARE(project->order, 3);
        QCOMPARE(project->folders.size(), std::size_t(1));
        QVERIFY(project->folders[0].main);
        QVERIFY(project->moduleSettings.contains(QStringLiteral("timetrack")));
        QCOMPARE(project->connectionOverrides.value(QStringLiteral("jira.cloud")), QStringLiteral("c-9"));
    }

    void priorityValues()
    {
        const Priority expected[] = {Priority::Urgent, Priority::High, Priority::Normal, Priority::Low};
        for (int value = 0; value < 4; ++value) {
            QJsonObject json = parse(R"({ "id": "p-1", "groupId": "g-1" })");
            json.insert(QStringLiteral("priority"), value);
            const auto project = Project::fromJson(json);
            QVERIFY(project);
            QCOMPARE(project->priority, expected[value]);
            QVERIFY(project->extra.isEmpty());
            QCOMPARE(project->toJson().value(QStringLiteral("priority")).toInt(), value);
        }
    }

    // Item

    void itemDefaults()
    {
        const auto item = Item::fromJson(parse(R"({ "id": "i-1", "type": "notes.note" })"));
        QVERIFY(item);
        // connectionId is omitted when empty.
        QCOMPARE(item->toJson(), parse(
            R"({ "id": "i-1", "type": "notes.note", "name": "", "config": {} })"));
    }

    void itemFields()
    {
        const auto item = Item::fromJson(parse(R"({ "id": "i-1", "type": "jira.issue",
            "name": "ACME-1", "config": { "key": "ACME-1" }, "connectionId": "c-3" })"));
        QVERIFY(item);
        QCOMPARE(item->type, QStringLiteral("jira.issue"));
        QCOMPARE(item->name, QStringLiteral("ACME-1"));
        QCOMPARE(item->config.value(QStringLiteral("key")).toString(), QStringLiteral("ACME-1"));
        QCOMPARE(item->connectionId, QStringLiteral("c-3"));
    }

    // Dashboard and RootDashboard

    void dashboardDefaults()
    {
        const auto dashboard = Dashboard::fromJson(parse(R"({ "id": "d-1" })"));
        QVERIFY(dashboard);
        QCOMPARE(dashboard->layout.kind, LayoutNode::Kind::Cell);
        QVERIFY(!dashboard->layout.ref);
        QCOMPARE(dashboard->toJson(), parse(
            R"({ "id": "d-1", "name": "", "layout": { "type": "cell", "ref": null } })"));
    }

    void dashboardUnusableLayout()
    {
        // An object of an unknown node type is an empty cell, written back unchanged.
        const auto unknown = Dashboard::fromJson(parse(
            R"({ "id": "d-1", "layout": { "type": "grid", "rows": 3 } })"));
        QVERIFY(unknown);
        QCOMPARE(unknown->layout.kind, LayoutNode::Kind::Cell);
        QVERIFY(!unknown->layout.ref);
        QCOMPARE(unknown->toJson().value(QStringLiteral("layout")).toObject(),
                 parse(R"({ "type": "grid", "rows": 3 })"));

        // A layout that is not an object is an empty cell, and the value is kept.
        const auto wrong = Dashboard::fromJson(parse(R"({ "id": "d-1", "layout": "split" })"));
        QVERIFY(wrong);
        QCOMPARE(wrong->layout.kind, LayoutNode::Kind::Cell);
        QVERIFY(!wrong->layout.ref);
        QCOMPARE(wrong->extra.value(QStringLiteral("layout")).toString(), QStringLiteral("split"));
        QCOMPARE(wrong->toJson().value(QStringLiteral("layout")).toString(), QStringLiteral("split"));
    }

    void rootDashboardDefaults()
    {
        const auto dashboard = RootDashboard::fromJson(parse(R"({ "id": "r-1" })"));
        QVERIFY(dashboard);
        QCOMPARE(dashboard->order, 0);
        QCOMPARE(dashboard->toJson(), parse(R"({ "id": "r-1", "name": "", "order": 0,
            "layout": { "type": "cell", "ref": null } })"));
    }

    void rootDashboardFields()
    {
        const auto dashboard = RootDashboard::fromJson(parse(R"({ "id": "r-1", "name": "Today",
            "order": 5, "layout": { "type": "cell", "ref": { "task": "t-1", "dashboard": "d-1" } } })"));
        QVERIFY(dashboard);
        QCOMPARE(dashboard->name, QStringLiteral("Today"));
        QCOMPARE(dashboard->order, 5);
        QVERIFY(dashboard->extra.isEmpty());
        QVERIFY(dashboard->layout.ref);
        QVERIFY(dashboard->layout.ref->isTask());
        QCOMPARE(dashboard->layout.ref->dashboard, QStringLiteral("d-1"));
    }

    // Task

    void taskDefaults()
    {
        const auto task = Task::fromJson(parse(R"({ "id": "t-1", "projectId": "p-1" })"));
        QVERIFY(task);
        QCOMPARE(task->state, TaskState::Active);
        QCOMPARE(task->priority, Priority::Normal);
        QVERIFY(!task->isDefault);
        QCOMPARE(task->toJson(), parse(R"({ "id": "t-1", "projectId": "p-1", "title": "",
            "isDefault": false, "state": "active", "priority": 2, "order": 0,
            "items": [], "dashboards": [], "defaultDashboardId": "" })"));
    }

    void taskFields()
    {
        const auto task = Task::fromJson(parse(FullTask));
        QVERIFY(task);
        QCOMPARE(task->title, QStringLiteral("Fix login"));
        QCOMPARE(task->state, TaskState::Done);
        QCOMPARE(task->priority, Priority::High);
        QCOMPARE(task->order, 4);
        QCOMPARE(task->items.size(), std::size_t(2));
        QCOMPARE(task->items[0].connectionId, QStringLiteral("c-2"));
        QCOMPARE(task->dashboards.size(), std::size_t(2));
        QCOMPARE(task->dashboards[0].layout.kind, LayoutNode::Kind::Split);
        QCOMPARE(task->defaultDashboardId, QStringLiteral("d-1"));
    }

    void taskStates_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<TaskState>("state");
        QTest::newRow("active") << QStringLiteral("active") << TaskState::Active;
        QTest::newRow("done") << QStringLiteral("done") << TaskState::Done;
        QTest::newRow("archived") << QStringLiteral("archived") << TaskState::Archived;
    }

    void taskStates()
    {
        QFETCH(QString, text);
        QFETCH(TaskState, state);
        QJsonObject json = parse(R"({ "id": "t-1", "projectId": "p-1", "isDefault": true })");
        json.insert(QStringLiteral("state"), text);
        const auto task = Task::fromJson(json);
        QVERIFY(task);
        QCOMPARE(task->state, state);
        QVERIFY(task->isDefault);
        QCOMPARE(task->toJson().value(QStringLiteral("state")).toString(), text);
    }

    // Unknown fields

    void unknownFieldsKept()
    {
        const auto group = Group::fromJson(parse(R"({ "id": "g-1", "color": "red",
            "schemaVersion": 1, "connections": [ { "id": "c-1", "token": { "x": 1 } } ] })"));
        QVERIFY(group);
        QCOMPARE(group->extra, parse(R"({ "color": "red", "schemaVersion": 1 })"));
        QCOMPARE(group->connections[0].extra, parse(R"({ "token": { "x": 1 } })"));
        const QJsonObject json = group->toJson();
        QCOMPARE(json.value(QStringLiteral("color")).toString(), QStringLiteral("red"));
        QCOMPARE(json.value(QStringLiteral("schemaVersion")).toInt(), 1);
        QCOMPARE(json.value(QStringLiteral("connections")).toArray().at(0).toObject()
                     .value(QStringLiteral("token")).toObject(), parse(R"({ "x": 1 })"));
    }

    // Wrong types

    void wrongTypesKept_data()
    {
        QTest::addColumn<QByteArray>("text");
        QTest::addColumn<QString>("key");

        // Each row is a Task, a Project or a Group with one wrong field.
        QTest::newRow("task title") << QByteArray(R"({ "title": 5 })") << QStringLiteral("title");
        QTest::newRow("task state") << QByteArray(R"({ "state": "paused" })") << QStringLiteral("state");
        QTest::newRow("task state number") << QByteArray(R"({ "state": 1 })") << QStringLiteral("state");
        QTest::newRow("task priority text") << QByteArray(R"({ "priority": "high" })") << QStringLiteral("priority");
        QTest::newRow("task priority range") << QByteArray(R"({ "priority": 7 })") << QStringLiteral("priority");
        QTest::newRow("task priority negative") << QByteArray(R"({ "priority": -1 })") << QStringLiteral("priority");
        QTest::newRow("task priority fraction") << QByteArray(R"({ "priority": 1.5 })") << QStringLiteral("priority");
        QTest::newRow("task order fraction") << QByteArray(R"({ "order": 2.5 })") << QStringLiteral("order");
        QTest::newRow("task order huge") << QByteArray(R"({ "order": 1e20 })") << QStringLiteral("order");
        QTest::newRow("task isDefault") << QByteArray(R"({ "isDefault": "yes" })") << QStringLiteral("isDefault");
        QTest::newRow("task items") << QByteArray(R"({ "items": { "a": 1 } })") << QStringLiteral("items");
        QTest::newRow("task dashboards") << QByteArray(R"({ "dashboards": null })") << QStringLiteral("dashboards");
        QTest::newRow("task defaultDashboardId") << QByteArray(R"({ "defaultDashboardId": [] })") << QStringLiteral("defaultDashboardId");
    }

    void wrongTypesKept()
    {
        QFETCH(QByteArray, text);
        QFETCH(QString, key);

        QJsonObject json = parse(text.constData());
        json.insert(QStringLiteral("id"), QStringLiteral("t-1"));
        json.insert(QStringLiteral("projectId"), QStringLiteral("p-1"));
        const auto task = Task::fromJson(json);
        QVERIFY(task);
        // The default is used.
        const Task defaults;
        QCOMPARE(task->title, defaults.title);
        QCOMPARE(task->state, defaults.state);
        QCOMPARE(task->priority, defaults.priority);
        QCOMPARE(task->order, defaults.order);
        QCOMPARE(task->isDefault, defaults.isDefault);
        QVERIFY(task->items.empty());
        QVERIFY(task->dashboards.empty());
        QCOMPARE(task->defaultDashboardId, defaults.defaultDashboardId);
        // The original value is kept and written back.
        QCOMPARE(task->extra.size(), 1);
        QCOMPARE(task->extra.value(key), json.value(key));
        QCOMPARE(task->toJson().value(key), json.value(key));
    }

    void wrongTypesInOtherEntities()
    {
        const QJsonObject projectJson = parse(R"({ "id": "p-1", "groupId": "g-1",
            "folders": "none", "moduleSettings": [1], "connectionOverrides": { "gitlab": 3 } })");
        const auto project = Project::fromJson(projectJson);
        QVERIFY(project);
        QVERIFY(project->folders.empty());
        QVERIFY(project->moduleSettings.isEmpty());
        QVERIFY(project->connectionOverrides.isEmpty());
        QCOMPARE(project->extra.size(), 3);
        QCOMPARE(project->toJson(), parse(R"({ "id": "p-1", "groupId": "g-1", "name": "",
            "priority": 2, "order": 0, "folders": "none", "moduleSettings": [1],
            "connectionOverrides": { "gitlab": 3 } })"));

        const QJsonObject groupJson = parse(R"({ "id": "g-1", "name": null, "order": "1",
            "connections": 0, "defaultConnections": [], "profileOverrides": "shared" })");
        const auto group = Group::fromJson(groupJson);
        QVERIFY(group);
        QVERIFY(group->name.isEmpty());
        QCOMPARE(group->order, 0);
        QCOMPARE(group->extra.size(), 5);
        QCOMPARE(group->toJson(), groupJson);

        const QJsonObject folderJson = parse(R"({ "id": "f-1", "main": 1, "remoteUrl": false })");
        const auto folder = Folder::fromJson(folderJson);
        QVERIFY(folder);
        QVERIFY(!folder->main);
        QVERIFY(folder->remoteUrl.isEmpty());
        QCOMPARE(folder->toJson(), parse(R"({ "id": "f-1", "name": "", "main": 1, "remoteUrl": false })"));

        const QJsonObject connectionJson = parse(R"({ "id": "c-1", "kind": 1, "baseUrl": {}, "fields": "x" })");
        const auto connection = Connection::fromJson(connectionJson);
        QVERIFY(connection);
        QVERIFY(connection->kind.isEmpty());
        QVERIFY(connection->fields.isEmpty());
        QCOMPARE(connection->toJson(), parse(
            R"({ "id": "c-1", "kind": 1, "name": "", "baseUrl": {}, "fields": "x" })"));

        const QJsonObject itemJson = parse(R"({ "id": "i-1", "type": "x", "config": "a=b", "connectionId": 4 })");
        const auto item = Item::fromJson(itemJson);
        QVERIFY(item);
        QVERIFY(item->config.isEmpty());
        QVERIFY(item->connectionId.isEmpty());
        QCOMPARE(item->toJson(), parse(R"({ "id": "i-1", "type": "x", "name": "", "config": "a=b", "connectionId": 4 })"));

        const auto root = RootDashboard::fromJson(parse(R"({ "id": "r-1", "order": "first" })"));
        QVERIFY(root);
        QCOMPARE(root->order, 0);
        QCOMPARE(root->toJson().value(QStringLiteral("order")).toString(), QStringLiteral("first"));
    }

    // Unusable data

    void badData_data()
    {
        QTest::addColumn<QString>("entity");
        QTest::addColumn<QByteArray>("text");
        QTest::addColumn<QString>("errorPart");

        QTest::newRow("folder no id") << "Folder" << QByteArray(R"({ "name": "a" })") << "\"id\"";
        QTest::newRow("connection no id") << "Connection" << QByteArray(R"({ "kind": "gitlab" })") << "\"id\"";
        QTest::newRow("group no id") << "Group" << QByteArray(R"({})") << "\"id\"";
        QTest::newRow("group id number") << "Group" << QByteArray(R"({ "id": 5 })") << "\"id\"";
        QTest::newRow("group id empty") << "Group" << QByteArray(R"({ "id": "" })") << "\"id\"";
        QTest::newRow("group bad connection") << "Group"
            << QByteArray(R"({ "id": "g", "connections": [ { "id": "c" }, { "kind": "x" } ] })")
            << "connections[1]: missing or invalid \"id\"";
        QTest::newRow("project no id") << "Project" << QByteArray(R"({ "groupId": "g" })") << "\"id\"";
        QTest::newRow("project no groupId") << "Project" << QByteArray(R"({ "id": "p" })") << "\"groupId\"";
        QTest::newRow("project bad folder") << "Project"
            << QByteArray(R"({ "id": "p", "groupId": "g", "folders": [ "backend" ] })")
            << "folders[0]: not an object";
        QTest::newRow("item no id") << "Item" << QByteArray(R"({ "type": "x" })") << "\"id\"";
        QTest::newRow("item no type") << "Item" << QByteArray(R"({ "id": "i" })") << "\"type\"";
        QTest::newRow("item type number") << "Item" << QByteArray(R"({ "id": "i", "type": 3 })") << "\"type\"";
        QTest::newRow("dashboard no id") << "Dashboard" << QByteArray(R"({ "name": "d" })") << "\"id\"";
        QTest::newRow("root dashboard no id") << "RootDashboard" << QByteArray(R"({ "order": 1 })") << "\"id\"";
        QTest::newRow("task no id") << "Task" << QByteArray(R"({ "projectId": "p" })") << "\"id\"";
        QTest::newRow("task no projectId") << "Task" << QByteArray(R"({ "id": "t" })") << "\"projectId\"";
        QTest::newRow("task bad item") << "Task"
            << QByteArray(R"({ "id": "t", "projectId": "p", "items": [ { "id": "i" } ] })")
            << "items[0]: missing or invalid \"type\"";
        QTest::newRow("task bad dashboard") << "Task"
            << QByteArray(R"({ "id": "t", "projectId": "p", "dashboards": [ {} ] })")
            << "dashboards[0]: missing or invalid \"id\"";
    }

    void badData()
    {
        QFETCH(QString, entity);
        QFETCH(QByteArray, text);
        QFETCH(QString, errorPart);

        const QJsonObject json = parse(text.constData());
        QString error;
        bool failed = false;
        if (entity == QLatin1String("Folder"))
            failed = !Folder::fromJson(json, &error) && !Folder::fromJson(json);
        else if (entity == QLatin1String("Connection"))
            failed = !Connection::fromJson(json, &error) && !Connection::fromJson(json);
        else if (entity == QLatin1String("Group"))
            failed = !Group::fromJson(json, &error) && !Group::fromJson(json);
        else if (entity == QLatin1String("Project"))
            failed = !Project::fromJson(json, &error) && !Project::fromJson(json);
        else if (entity == QLatin1String("Item"))
            failed = !Item::fromJson(json, &error) && !Item::fromJson(json);
        else if (entity == QLatin1String("Dashboard"))
            failed = !Dashboard::fromJson(json, &error) && !Dashboard::fromJson(json);
        else if (entity == QLatin1String("RootDashboard"))
            failed = !RootDashboard::fromJson(json, &error) && !RootDashboard::fromJson(json);
        else if (entity == QLatin1String("Task"))
            failed = !Task::fromJson(json, &error) && !Task::fromJson(json);
        QVERIFY(failed);
        QVERIFY2(error.contains(errorPart), qPrintable(error));
    }

    // Round trips

    void roundTrips_data()
    {
        QTest::addColumn<QString>("entity");
        QTest::addColumn<QByteArray>("text");

        QTest::newRow("task") << "Task" << QByteArray(FullTask);
        QTest::newRow("group") << "Group" << QByteArray(R"({ "id": "g-1", "name": "Work", "order": 1,
            "connections": [
                { "id": "c-1", "kind": "gitlab", "name": "gitlab.com", "baseUrl": "https:\/\/gitlab.com",
                  "fields": {} },
                { "id": "c-2", "kind": "timetrack", "name": "Hours", "baseUrl": "https:\/\/t.example",
                  "fields": { "team": 4 }, "region": "eu" } ],
            "defaultConnections": { "gitlab": "c-1", "timetrack": "c-2" },
            "profileOverrides": { "gitlab.com": "shared" }, "schemaVersion": 1 })");
        QTest::newRow("project") << "Project" << QByteArray(R"({ "id": "p-1", "groupId": "g-1",
            "name": "Backend", "priority": 3, "order": 7,
            "folders": [ { "id": "f-1", "name": "backend", "remoteUrl": "git@x:a/b.git", "main": true },
                         { "id": "f-2", "name": "docs", "main": false, "color": 2 } ],
            "moduleSettings": { "gitlab": { "project": "acme/backend" }, "timetrack": { "code": "X" } },
            "connectionOverrides": { "jira.cloud": "c-9" }, "archivedAt": null })");
        QTest::newRow("root dashboard") << "RootDashboard" << QByteArray(R"({ "id": "r-1",
            "name": "Today", "order": 2, "layout": { "type": "split", "dir": "v", "sizes": [1, 3],
              "children": [
                { "type": "cell", "ref": { "task": "t-1", "dashboard": "d-1" } },
                { "type": "cell", "ref": { "item": "i-1", "task": "t-2" } } ] } })");
        QTest::newRow("wrong types") << "Task" << QByteArray(R"({ "id": "t-1", "projectId": "p-1",
            "title": 1, "isDefault": "no", "state": "later", "priority": 9, "order": "x",
            "items": {}, "dashboards": "none", "defaultDashboardId": false })");
    }

    void roundTrips()
    {
        QFETCH(QString, entity);
        QFETCH(QByteArray, text);

        const QJsonObject json = parse(text.constData());
        QJsonObject result;
        if (entity == QLatin1String("Task"))
            result = roundTrip<Task>(json);
        else if (entity == QLatin1String("Group"))
            result = roundTrip<Group>(json);
        else if (entity == QLatin1String("Project"))
            result = roundTrip<Project>(json);
        else if (entity == QLatin1String("RootDashboard"))
            result = roundTrip<RootDashboard>(json);
        QCOMPARE(result, json);
        // Writing is stable.
        QCOMPARE(QJsonDocument(result).toJson(), QJsonDocument(json).toJson());
    }
};

QTEST_GUILESS_MAIN(TestEntities)
#include "tst_entities.moc"
