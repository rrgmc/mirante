// Tests the profile migration of decisions 0050 and 0054: connections, in
// groups or in profiles, become the GitLab and Jira base URLs of the profile
// each group uses, the default connection of a kind winning; connections of
// other kinds are kept; module settings at the top of workspace.mirante move
// to Default; running it again changes nothing; and the Store writes the
// migrated files once, on open and on reload. The fixtures in
// fixtures/legacy-profiles are the sample data's group files and
// workspace.mirante from before decision 0050; migrated, they must give the
// sample data's current files.
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

QJsonObject connection(const QString &id, const QString &kind, const QString &baseUrl)
{
    return QJsonObject{{QStringLiteral("id"), id},
                       {QStringLiteral("kind"), kind},
                       {QStringLiteral("name"), id},
                       {QStringLiteral("baseUrl"), baseUrl},
                       {QStringLiteral("fields"), QJsonObject()}};
}

// The old keys, in a group's or a profile's extra.
QJsonObject oldKeys(const QJsonArray &connections, const QJsonObject &defaults)
{
    return QJsonObject{{profilemigration::kOldConnections, connections},
                       {profilemigration::kOldDefaultConnections, defaults}};
}

Group oldGroup(const QString &id, int order, const QString &profile, const QJsonArray &connections,
               const QJsonObject &defaults)
{
    Group group;
    group.id = id;
    group.name = id;
    group.order = order;
    group.profile = profile;
    group.extra = oldKeys(connections, defaults);
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

// The base URL of a module's General Configuration in a profile.
QString baseUrl(const Profile &profile, const QString &module)
{
    return profile.moduleSettings.value(module)
        .toObject()
        .value(QStringLiteral("general"))
        .toObject()
        .value(QStringLiteral("baseUrl"))
        .toString();
}

const QString gitlab = QStringLiteral("gitlab");
const QString jira = QStringLiteral("jira");
const QString cloud = QStringLiteral("jira.cloud");
const QString dc = QStringLiteral("jira.dc");

} // namespace

class TestProfileMigration : public QObject
{
    Q_OBJECT

private slots:
    // Each group's connections go to the profile it uses: a group without
    // one, or with one that is not listed, uses Default. The default of the
    // kind wins, else the first; Jira Cloud before Data Center. The old keys
    // leave the groups.
    void groupsGiveBaseUrls()
    {
        std::vector<Group> groups{
            oldGroup(QStringLiteral("g1"), 0, QStringLiteral("work"),
                     QJsonArray{connection(QStringLiteral("c1"), gitlab, QStringLiteral("https://gitlab.com")),
                                connection(QStringLiteral("c2"), gitlab, QStringLiteral("https://git.acme.example")),
                                connection(QStringLiteral("c3"), dc, QStringLiteral("https://jira.acme.example")),
                                connection(QStringLiteral("c4"), cloud, QStringLiteral("https://acme.atlassian.net"))},
                     QJsonObject{{gitlab, QStringLiteral("c2")}, {cloud, QStringLiteral("c4")},
                                 {dc, QStringLiteral("c3")}}),
            oldGroup(QStringLiteral("g2"), 1, QString(),
                     QJsonArray{connection(QStringLiteral("c5"), gitlab, QStringLiteral("https://gitlab.com")),
                                connection(QStringLiteral("c6"), dc, QStringLiteral("https://jira.home.example"))},
                     QJsonObject{}),
            oldGroup(QStringLiteral("g3"), 2, QStringLiteral("gone"), QJsonArray{}, QJsonObject{})};
        groups[2].extra.insert(QStringLiteral("futureField"), 1);
        std::vector<Profile> profiles{makeProfile(QStringLiteral("work"), QStringLiteral("Work"))};

        const Changes changes = profilemigration::moveConnections(groups, profiles);
        QVERIFY(changes.profiles);
        QCOMPARE(changes.groups, (QStringList{QStringLiteral("g1"), QStringLiteral("g2"), QStringLiteral("g3")}));

        QCOMPARE(profiles.size(), std::size_t(2));
        QCOMPARE(profiles[0].id, kDefaultProfile);
        const Profile *work = find(profiles, QStringLiteral("work"));
        QCOMPARE(baseUrl(*work, gitlab), QStringLiteral("https://git.acme.example"));
        QCOMPARE(baseUrl(*work, jira), QStringLiteral("https://acme.atlassian.net"));
        QVERIFY(work->extra.isEmpty());
        const Profile *standard = find(profiles, kDefaultProfile);
        QCOMPARE(baseUrl(*standard, gitlab), QStringLiteral("https://gitlab.com"));
        QCOMPARE(baseUrl(*standard, jira), QStringLiteral("https://jira.home.example"));

        for (const Group &group : groups)
            QVERIFY(!profilemigration::hasOldConnections(group));
        QCOMPARE(groups[2].extra, (QJsonObject{{QStringLiteral("futureField"), 1}}));
    }

    // Connections kept in a profile by decision 0050 move the same way. A
    // base URL the profile already has wins. Other kinds stay, under the old
    // keys, with their default.
    void profileConnectionsGiveBaseUrls()
    {
        Profile work = makeProfile(QStringLiteral("work"), QStringLiteral("Work"));
        work.extra = oldKeys(
            QJsonArray{connection(QStringLiteral("c1"), gitlab, QStringLiteral("https://gitlab.com")),
                       connection(QStringLiteral("c2"), cloud, QStringLiteral("https://acme.atlassian.net")),
                       connection(QStringLiteral("t1"), QStringLiteral("timetrack"), QStringLiteral("https://t.example"))},
            QJsonObject{{gitlab, QStringLiteral("c1")}, {QStringLiteral("timetrack"), QStringLiteral("t1")}});
        work.moduleSettings = parse(R"({ "jira": { "general": { "baseUrl": "https:\/\/mine.example" } } })");
        std::vector<Group> groups;
        std::vector<Profile> profiles{work};

        QVERIFY(profilemigration::hasOldConnections(profiles[0]));
        const Changes changes = profilemigration::moveConnections(groups, profiles);
        QVERIFY(changes.profiles);
        const Profile *moved = find(profiles, QStringLiteral("work"));
        QCOMPARE(baseUrl(*moved, gitlab), QStringLiteral("https://gitlab.com"));
        QCOMPARE(baseUrl(*moved, jira), QStringLiteral("https://mine.example"));
        const QJsonArray kept = moved->extra.value(profilemigration::kOldConnections).toArray();
        QCOMPARE(kept.size(), 1);
        QCOMPARE(kept.at(0).toObject().value(QStringLiteral("id")).toString(), QStringLiteral("t1"));
        QCOMPARE(moved->extra.value(profilemigration::kOldDefaultConnections).toObject(),
                 (QJsonObject{{QStringLiteral("timetrack"), QStringLiteral("t1")}}));
        QVERIFY(!profilemigration::hasOldConnections(*moved));

        // Again: nothing changes, and the other kind stays once.
        const std::vector<Profile> once = profiles;
        QVERIFY(!profilemigration::moveConnections(groups, profiles).any());
        QCOMPARE(profiles, once);
    }

    // Running it again changes nothing. Groups whose old keys came back, for
    // example from a device with an older version, only lose them.
    void runsOnce()
    {
        const Group original = oldGroup(
            QStringLiteral("g1"), 0, QString(),
            QJsonArray{connection(QStringLiteral("c1"), gitlab, QStringLiteral("https://gitlab.com"))},
            QJsonObject{{gitlab, QStringLiteral("c1")}});
        std::vector<Group> groups{original};
        std::vector<Profile> profiles;
        QVERIFY(profilemigration::moveConnections(groups, profiles).any());
        const std::vector<Profile> once = profiles;

        Changes again = profilemigration::moveConnections(groups, profiles);
        QVERIFY(!again.any());
        QCOMPARE(profiles, once);

        groups = {original};
        again = profilemigration::moveConnections(groups, profiles);
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
        std::vector<Profile> profiles;
        QVERIFY(!profilemigration::moveConnections(groups, profiles).any());
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

        // The Work profile talks to gitlab.com and Acme's Jira Cloud.
        const Profile *work = migrated.profile(QStringLiteral("3f6c2a8e-5d1b-4c7a-9e2f-8b0d4a6c1e93"));
        QVERIFY(work);
        QCOMPARE(baseUrl(*work, gitlab), QStringLiteral("https://gitlab.com"));
        QCOMPARE(baseUrl(*work, jira), QStringLiteral("https://acme.atlassian.net"));
    }

    // Opening the folder writes the migrated files once: workspace.mirante
    // with the base URLs, and the groups without connections. They then equal the
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
        QVERIFY(model.profile(kDefaultProfile)->moduleSettings.isEmpty());
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
        QCOMPARE(baseUrl(*work, gitlab), QStringLiteral("https://g.example"));
        QVERIFY(!profilemigration::hasOldConnections(*model.group(QStringLiteral("g1"))));

        store.flush();
        const QJsonObject group = readJson(groupFile);
        QVERIFY(!group.contains(QStringLiteral("connections")));
        QCOMPARE(group.value(QStringLiteral("profile")).toString(), QStringLiteral("work"));
        const QJsonArray profiles = readJson(dir.filePath(QStringLiteral("workspace.mirante")))
                                        .value(QStringLiteral("profiles"))
                                        .toArray();
        // Default comes first: the file is from before decision 0057.
        QCOMPARE(profiles.size(), 2);
        QCOMPARE(profiles.at(0).toObject().value(QStringLiteral("id")).toString(), kDefaultProfile);
        QVERIFY(!profiles.at(1).toObject().contains(QStringLiteral("connections")));
        QCOMPARE(profiles.at(1)
                     .toObject()
                     .value(QStringLiteral("moduleSettings"))
                     .toObject()
                     .value(gitlab)
                     .toObject()
                     .value(QStringLiteral("general"))
                     .toObject()
                     .value(QStringLiteral("baseUrl"))
                     .toString(),
                 QStringLiteral("https://g.example"));
    }
};

QTEST_MAIN(TestProfileMigration)
#include "tst_profile_migration.moc"
