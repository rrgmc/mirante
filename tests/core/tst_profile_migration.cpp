// Tests the profile migration of decision 0050: groups' connections move to
// the profile each group uses, with distinct names and project overrides
// where two groups clash, module settings at the top of workspace.mirante
// move to Default, running it again changes nothing, and the Store writes the
// migrated files once, on open and on reload. The fixtures in
// fixtures/legacy-profiles are the sample data's group files and
// workspace.mirante from before the change; migrated, they must give the
// sample data's current files.
#include "core/Connections.h"
#include "core/Model.h"
#include "core/ProfileMigration.h"
#include "core/Store.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

using namespace mirante;
using profilemigration::Changes;

namespace {

constexpr int kTimeout = 10000;

// The fixtures from before decision 0050, next to this file.
QString legacyDir()
{
    return QStringLiteral(MIRANTE_FIXTURES_DIR) + QStringLiteral("/../../tests/core/fixtures/legacy-profiles");
}

QJsonObject parse(const char *text) { return QJsonDocument::fromJson(QByteArray(text)).object(); }

QJsonObject readJson(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}

void writeFile(const QString &path, const QByteArray &content)
{
    QVERIFY(QDir().mkpath(QFileInfo(path).path()));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(content), qint64(content.size()));
}

// Copies every file under from into to, keeping the folders.
void copyTree(const QString &from, const QString &to)
{
    QDirIterator it(from, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString source = it.next();
        const QString target = to + QLatin1Char('/') + QDir(from).relativeFilePath(source);
        QVERIFY(QDir().mkpath(QFileInfo(target).path()));
        QFile::remove(target);
        QVERIFY2(QFile::copy(source, target), qPrintable(source));
    }
}

Connection makeConnection(const QString &id, const QString &kind, const QString &name)
{
    Connection connection;
    connection.id = id;
    connection.kind = kind;
    connection.name = name;
    connection.baseUrl = QStringLiteral("https://%1.example.com").arg(id);
    return connection;
}

// A group from before decision 0050, with its connections in extra, as
// Group::fromJson leaves them.
Group oldGroup(const QString &id, const QString &name, int order, const QString &profile,
               const std::vector<Connection> &connections, const QJsonObject &defaults)
{
    Group group;
    group.id = id;
    group.name = name;
    group.order = order;
    group.profile = profile;
    QJsonArray array;
    for (const Connection &connection : connections)
        array.append(connection.toJson());
    group.extra.insert(profilemigration::kOldConnections, array);
    group.extra.insert(profilemigration::kOldDefaultConnections, defaults);
    return group;
}

Profile makeProfile(const QString &id, const QString &name)
{
    Profile profile;
    profile.id = id;
    profile.name = name;
    return profile;
}

const Profile *find(const std::vector<Profile> &profiles, const QString &id)
{
    for (const Profile &profile : profiles) {
        if (profile.id == id)
            return &profile;
    }
    return nullptr;
}

QStringList namesOf(const Profile &profile)
{
    QStringList names;
    for (const Connection &connection : profile.connections)
        names << connection.name;
    return names;
}

const QString gitlab = QStringLiteral("gitlab");
const QString cloud = QStringLiteral("jira.cloud");

} // namespace

class TestProfileMigration : public QObject
{
    Q_OBJECT

private slots:
    // Each group's connections go to the profile it uses, keeping their ids,
    // with its defaults. A group without a profile, or with one that is not
    // listed, uses Default. The old keys leave the groups.
    void movesToTheGroupsProfile()
    {
        std::vector<Group> groups{
            oldGroup(QStringLiteral("g1"), QStringLiteral("Work"), 0, QStringLiteral("work"),
                     {makeConnection(QStringLiteral("c1"), gitlab, QStringLiteral("gitlab.com (work)")),
                      makeConnection(QStringLiteral("c2"), cloud, QStringLiteral("Acme Jira"))},
                     QJsonObject{{gitlab, QStringLiteral("c1")}, {cloud, QStringLiteral("c2")}}),
            oldGroup(QStringLiteral("g2"), QStringLiteral("Home"), 1, QString(),
                     {makeConnection(QStringLiteral("c3"), gitlab, QStringLiteral("gitlab.com (home)"))},
                     QJsonObject{{gitlab, QStringLiteral("c3")}}),
            oldGroup(QStringLiteral("g3"), QStringLiteral("Lost"), 2, QStringLiteral("gone"),
                     {makeConnection(QStringLiteral("c4"), cloud, QStringLiteral("Lost Jira"))}, QJsonObject{})};
        groups[2].extra.insert(QStringLiteral("futureField"), 1);
        std::vector<Project> projects;
        std::vector<Profile> profiles{makeProfile(QStringLiteral("work"), QStringLiteral("Work"))};

        const Changes changes = profilemigration::moveConnections(groups, projects, profiles);
        QVERIFY(changes.profiles);
        QCOMPARE(changes.groups, (QStringList{QStringLiteral("g1"), QStringLiteral("g2"), QStringLiteral("g3")}));
        QVERIFY(changes.projects.isEmpty());

        // Default was added first.
        QCOMPARE(profiles.size(), std::size_t(2));
        QCOMPARE(profiles[0].id, kDefaultProfile);
        const Profile *work = find(profiles, QStringLiteral("work"));
        QCOMPARE(namesOf(*work), (QStringList{QStringLiteral("gitlab.com (work)"), QStringLiteral("Acme Jira")}));
        QCOMPARE(work->connections[0].id, QStringLiteral("c1"));
        QCOMPARE(work->defaultConnections.value(gitlab), QStringLiteral("c1"));
        QCOMPARE(work->defaultConnections.value(cloud), QStringLiteral("c2"));
        const Profile *standard = find(profiles, kDefaultProfile);
        QCOMPARE(namesOf(*standard), (QStringList{QStringLiteral("gitlab.com (home)"), QStringLiteral("Lost Jira")}));
        QCOMPARE(standard->defaultConnections.value(gitlab), QStringLiteral("c3"));

        for (const Group &group : groups) {
            QVERIFY(!profilemigration::hasOldConnections(group));
            QVERIFY(!group.toJson().contains(QStringLiteral("connections")));
        }
        QCOMPARE(groups[2].extra, (QJsonObject{{QStringLiteral("futureField"), 1}}));
    }

    // Two groups in one profile: a name already used gets the group's name,
    // and again a number. The first group by order keeps its defaults; the
    // other's projects get its default as an override, unless they have one.
    void twoGroupsInOneProfile()
    {
        std::vector<Group> groups{
            oldGroup(QStringLiteral("g2"), QStringLiteral("Client"), 1, QStringLiteral("work"),
                     {makeConnection(QStringLiteral("c3"), gitlab, QStringLiteral("gitlab.com")),
                      makeConnection(QStringLiteral("c4"), gitlab, QStringLiteral("GitLab.com (Client)")),
                      makeConnection(QStringLiteral("c5"), cloud, QStringLiteral("Jira"))},
                     QJsonObject{{gitlab, QStringLiteral("c3")}, {cloud, QStringLiteral("c5")}}),
            oldGroup(QStringLiteral("g1"), QStringLiteral("Work"), 0, QStringLiteral("work"),
                     {makeConnection(QStringLiteral("c1"), gitlab, QStringLiteral("gitlab.com"))},
                     QJsonObject{{gitlab, QStringLiteral("c1")}})};
        Project plain;
        plain.id = QStringLiteral("p1");
        plain.groupId = QStringLiteral("g2");
        Project overridden = plain;
        overridden.id = QStringLiteral("p2");
        overridden.connectionOverrides.insert(gitlab, QStringLiteral("c4"));
        Project ofWork = plain;
        ofWork.id = QStringLiteral("p3");
        ofWork.groupId = QStringLiteral("g1");
        std::vector<Project> projects{plain, overridden, ofWork};
        std::vector<Profile> profiles{makeProfile(kDefaultProfile, QString()),
                                      makeProfile(QStringLiteral("work"), QStringLiteral("Work"))};

        const Changes changes = profilemigration::moveConnections(groups, projects, profiles);
        const Profile *work = find(profiles, QStringLiteral("work"));
        // g1 comes first by its order.
        QCOMPARE(namesOf(*work), (QStringList{QStringLiteral("gitlab.com"), QStringLiteral("gitlab.com (Client)"),
                                              QStringLiteral("GitLab.com (Client) (Client)"), QStringLiteral("Jira")}));
        QCOMPARE(work->defaultConnections.value(gitlab), QStringLiteral("c1"));
        QCOMPARE(work->defaultConnections.value(cloud), QStringLiteral("c5"));
        QCOMPARE(changes.projects, QStringList{QStringLiteral("p1")});
        QCOMPARE(projects[0].connectionOverrides.value(gitlab), QStringLiteral("c3"));
        QVERIFY(!projects[0].connectionOverrides.contains(cloud));
        QCOMPARE(projects[1].connectionOverrides.value(gitlab), QStringLiteral("c4"));
        QVERIFY(projects[2].connectionOverrides.isEmpty());
        QVERIFY(find(profiles, kDefaultProfile)->connections.empty());
    }

    // A name taken twice gets a number after the group's name.
    void numberedNames()
    {
        std::vector<Group> groups{oldGroup(QStringLiteral("g1"), QStringLiteral("Work"), 0, QString(),
                                           {makeConnection(QStringLiteral("c9"), gitlab, QStringLiteral("G"))},
                                           QJsonObject{})};
        std::vector<Project> projects;
        Profile standard = makeProfile(kDefaultProfile, QString());
        standard.connections = {makeConnection(QStringLiteral("c1"), gitlab, QStringLiteral("G")),
                                makeConnection(QStringLiteral("c2"), gitlab, QStringLiteral("G (Work)"))};
        std::vector<Profile> profiles{standard};
        profilemigration::moveConnections(groups, projects, profiles);
        QCOMPARE(profiles[0].connections.back().name, QStringLiteral("G (Work 2)"));
    }

    // Running it again changes nothing. Groups whose old keys came back, for
    // example from a device with an older version, only lose them.
    void runsOnce()
    {
        const Group original = oldGroup(QStringLiteral("g1"), QStringLiteral("Work"), 0, QString(),
                                        {makeConnection(QStringLiteral("c1"), gitlab, QStringLiteral("G"))},
                                        QJsonObject{{gitlab, QStringLiteral("c1")}});
        std::vector<Group> groups{original};
        std::vector<Project> projects;
        std::vector<Profile> profiles;
        QVERIFY(profilemigration::moveConnections(groups, projects, profiles).any());
        const std::vector<Profile> once = profiles;

        Changes again = profilemigration::moveConnections(groups, projects, profiles);
        QVERIFY(!again.any());
        QCOMPARE(profiles, once);

        groups = {original};
        again = profilemigration::moveConnections(groups, projects, profiles);
        QVERIFY(!again.profiles);
        QCOMPARE(again.groups, QStringList{QStringLiteral("g1")});
        QCOMPARE(profiles, once);
        QVERIFY(!profilemigration::hasOldConnections(groups[0]));
    }

    // A group whose old keys cannot be read keeps them, and nothing moves.
    void unreadableKeysStay()
    {
        Group group;
        group.id = QStringLiteral("g1");
        group.extra = parse(R"({ "connections": [ { "kind": "gitlab" } ], "defaultConnections": {} })");
        Group notArray;
        notArray.id = QStringLiteral("g2");
        notArray.extra = parse(R"({ "connections": 3 })");
        std::vector<Group> groups{group, notArray};
        std::vector<Project> projects;
        std::vector<Profile> profiles;
        QVERIFY(!profilemigration::moveConnections(groups, projects, profiles).any());
        QCOMPARE(groups[0].extra, group.extra);
        QCOMPARE(groups[1].extra, notArray.extra);
    }

    // Module settings from the top of workspace.mirante go to Default. A
    // screen that Default has keeps Default's values.
    void moduleSettingsGoToDefault()
    {
        std::vector<Profile> profiles{makeProfile(QStringLiteral("work"), QStringLiteral("Work"))};
        const QJsonObject old = parse(R"({ "gitlab": { "general": { "projects": [ "a/b" ] }, "look": { "x": 1 } },
                                           "missing": { "general": { "y": "z" } } })");
        QVERIFY(profilemigration::moveModuleSettings(old, profiles));
        QCOMPARE(profiles[0].id, kDefaultProfile);
        QCOMPARE(profiles[0].moduleSettings, old);
        QVERIFY(profiles[1].moduleSettings.isEmpty());

        // Again: nothing new.
        QVERIFY(!profilemigration::moveModuleSettings(old, profiles));

        profiles[0].moduleSettings = parse(R"({ "gitlab": { "general": { "projects": [ "mine" ] } } })");
        QVERIFY(profilemigration::moveModuleSettings(old, profiles));
        QCOMPARE(profiles[0].moduleSettings, parse(R"({ "gitlab": { "general": { "projects": [ "mine" ] },
                                                                    "look": { "x": 1 } },
                                                        "missing": { "general": { "y": "z" } } })"));
        QVERIFY(!profilemigration::moveModuleSettings({}, profiles));
    }

    // store::load() migrates in the model only. The sample data's old files
    // give its current content.
    void loadMigratesInTheModel()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        copyTree(QStringLiteral(MIRANTE_FIXTURES_DIR), dir.path());
        copyTree(legacyDir(), dir.path());
        const QJsonObject oldGroup = readJson(dir.filePath(QStringLiteral("groups/6d1b9bb9-f4c3-5677-a4e1-683ba618d4c7.json")));
        QVERIFY(oldGroup.contains(QStringLiteral("connections")));

        Model migrated;
        QVERIFY(store::load(dir.path(), migrated).empty());
        Model current;
        QVERIFY(store::load(QStringLiteral(MIRANTE_FIXTURES_DIR), current).empty());
        QCOMPARE(store::serialise(migrated), store::serialise(current));
        // Nothing was written.
        QCOMPARE(readJson(dir.filePath(QStringLiteral("groups/6d1b9bb9-f4c3-5677-a4e1-683ba618d4c7.json"))), oldGroup);

        // An item's connection still resolves, now through its profile.
        const QString taskId = QStringLiteral("16d3c8ff-5fa8-5cf0-8090-c62592d1bd96");
        const Task *task = migrated.task(taskId);
        QVERIFY(task);
        const Item *item = nullptr;
        for (const Item &candidate : task->items) {
            if (candidate.connectionId == QLatin1String("90d3f914-a970-57dc-ab52-a7fb83e824fc"))
                item = &candidate;
        }
        QVERIFY(item);
        const Connection *connection =
            connections::resolve(migrated, taskId, *item, {QStringLiteral("jira.cloud"), QStringLiteral("jira.dc")});
        QVERIFY(connection);
        QCOMPARE(connection->name, QStringLiteral("Ops Jira (Data Center)"));
    }

    // Opening the folder writes the migrated files once: workspace.mirante
    // with the connections, and the groups without them. They then equal the
    // sample data's current files. Opening again changes nothing.
    void openWritesOnce()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        copyTree(QStringLiteral(MIRANTE_FIXTURES_DIR), dir.path());
        copyTree(legacyDir(), dir.path());

        Model model;
        {
            Store store(model);
            QVERIFY(store.open(dir.path()).empty());
            store.flush();
        }
        const QStringList files{QStringLiteral("workspace.mirante"),
                                QStringLiteral("groups/6d1b9bb9-f4c3-5677-a4e1-683ba618d4c7.json"),
                                QStringLiteral("groups/80314e00-c02f-5f8b-977f-0756b76406ea.json")};
        for (const QString &file : files) {
            const QJsonObject written = readJson(dir.filePath(file));
            const QJsonObject expected = readJson(QStringLiteral(MIRANTE_FIXTURES_DIR) + QLatin1Char('/') + file);
            if (written != expected)
                qWarning().noquote() << file << QJsonDocument(written).toJson();
            QVERIFY2(written == expected, qPrintable(file));
        }

        // Opened again: the same files, nothing to move.
        QByteArray before;
        {
            QFile workspace(dir.filePath(QStringLiteral("workspace.mirante")));
            QVERIFY(workspace.open(QIODevice::ReadOnly));
            before = workspace.readAll();
        }
        Model again;
        Store store(again);
        QVERIFY(store.open(dir.path()).empty());
        store.flush();
        QFile workspace(dir.filePath(QStringLiteral("workspace.mirante")));
        QVERIFY(workspace.open(QIODevice::ReadOnly));
        QCOMPARE(workspace.readAll(), before);
        QCOMPARE(store::serialise(again), store::serialise(model));
    }

    // A workspace.mirante from a newer version cannot be written, so nothing
    // moves, and the groups keep their connections on disk.
    void newerWorkspaceStopsTheMigration()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        copyTree(legacyDir(), dir.path());
        writeFile(dir.filePath(QStringLiteral("workspace.mirante")), R"({ "schemaVersion": 99, "name": "New" })");
        const QString groupFile = dir.filePath(QStringLiteral("groups/80314e00-c02f-5f8b-977f-0756b76406ea.json"));
        const QJsonObject before = readJson(groupFile);

        Model model;
        Store store(model);
        QVERIFY(!store.open(dir.path()).empty());
        store.flush();
        QCOMPARE(readJson(groupFile), before);
        QVERIFY(model.profile(kDefaultProfile)->connections.empty());
        QVERIFY(profilemigration::hasOldConnections(*model.group(QStringLiteral("80314e00-c02f-5f8b-977f-0756b76406ea"))));
    }

    // A group file with connections that arrives later, for example from a
    // device with an older version, is migrated too, and written once.
    void reloadMigratesAGroup()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        writeFile(dir.filePath(QStringLiteral("workspace.mirante")),
                  R"({ "schemaVersion": 1, "name": "W", "profiles": [ { "id": "work", "name": "Work" } ] })");
        Model model;
        Store store(model);
        QVERIFY(store.open(dir.path()).empty());

        const QString groupFile = dir.filePath(QStringLiteral("groups/g1.json"));
        writeFile(groupFile, R"({ "schemaVersion": 1, "id": "g1", "name": "Work", "order": 0, "webProfile": "work",
            "connections": [ { "id": "c1", "kind": "gitlab", "name": "G", "baseUrl": "https:\/\/g.example",
                               "fields": {} } ],
            "defaultConnections": { "gitlab": "c1" } })");
        QTRY_VERIFY_WITH_TIMEOUT(model.group(QStringLiteral("g1")), kTimeout);
        const Profile *work = model.profile(QStringLiteral("work"));
        QCOMPARE(work->connections.size(), std::size_t(1));
        QCOMPARE(work->defaultConnections.value(gitlab), QStringLiteral("c1"));
        QVERIFY(!profilemigration::hasOldConnections(*model.group(QStringLiteral("g1"))));

        store.flush();
        const QJsonObject group = readJson(groupFile);
        QVERIFY(!group.contains(QStringLiteral("connections")));
        QCOMPARE(group.value(QStringLiteral("profile")).toString(), QStringLiteral("work"));
        const QJsonArray profiles = readJson(dir.filePath(QStringLiteral("workspace.mirante")))
                                        .value(QStringLiteral("profiles"))
                                        .toArray();
        QCOMPARE(profiles.size(), 1);
        QCOMPARE(profiles.at(0).toObject().value(QStringLiteral("connections")).toArray().size(), 1);
    }
};

QTEST_MAIN(TestProfileMigration)
#include "tst_profile_migration.moc"
