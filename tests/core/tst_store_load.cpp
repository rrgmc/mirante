// Tests store::load(): loading the fixtures, and the list of load problems.
#include "core/Model.h"
#include "core/Store.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <cstddef>

using namespace mirante;

namespace {

void writeFile(const QString &dir, const QString &path, const QByteArray &content)
{
    QVERIFY(QDir(dir).mkpath(QFileInfo(path).path()));
    QFile file(dir + QLatin1Char('/') + path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(content), qint64(content.size()));
}

} // namespace

class TestStoreLoad : public QObject
{
    Q_OBJECT

private slots:
    void loadsFixtures();
    void fixtureDetails();
    void reportsProblems();
    void emptyFolder();
    void replacesModel();
    void workspaceName();
    void profiles();
    void oldWebProfiles();
    void moduleSettings();
};

void TestStoreLoad::loadsFixtures()
{
    Model model;
    QSignalSpy spy(&model, &Model::modelReset);
    const std::vector<LoadProblem> problems = store::load(QStringLiteral(MIRANTE_FIXTURES_DIR), model);

    for (const LoadProblem &problem : problems)
        qWarning() << problem.file << problem.message;
    QVERIFY(problems.empty());
    QCOMPARE(spy.count(), 1);

    QCOMPARE(model.groups().size(), std::size_t(2));
    std::size_t projects = 0;
    std::size_t tasks = 0;
    std::size_t items = 0;
    std::size_t dashboards = 0;
    for (const Group *group : model.groups()) {
        for (const Project *project : model.projectsOfGroup(group->id)) {
            ++projects;
            for (const Task *task : model.tasksOfProject(project->id)) {
                ++tasks;
                items += task->items.size();
                dashboards += task->dashboards.size();
            }
        }
    }
    QCOMPARE(projects, std::size_t(5));
    QCOMPARE(tasks, std::size_t(17));
    QCOMPARE(items, std::size_t(44));
    QCOMPARE(dashboards, std::size_t(18));
    QCOMPARE(model.rootDashboards().size(), std::size_t(2));
}

void TestStoreLoad::fixtureDetails()
{
    Model model;
    QVERIFY(store::load(QStringLiteral(MIRANTE_FIXTURES_DIR), model).empty());

    const Task *twoDashboards = model.task(QStringLiteral("760c8e20-8555-5a2c-bcbe-75e7fd08ca74"));
    QVERIFY(twoDashboards);
    QCOMPARE(twoDashboards->dashboards.size(), std::size_t(2));

    QCOMPARE(model.rootDashboard(QStringLiteral("60139a76-dd45-50ea-890b-ae022026e421"))->name,
             QStringLiteral("Morning"));
    QCOMPARE(model.rootDashboard(QStringLiteral("5247a9e5-ee54-5552-8831-0b2ff361afba"))->name,
             QStringLiteral("Release"));

    // The item of the missing timetrack module is loaded like any other.
    const QString timetrackTask = QStringLiteral("d71bd575-6d1b-5088-9da0-7d2cebf0d59e");
    const Item *entry = model.item(timetrackTask, QStringLiteral("627898f1-6342-5d8c-ac0e-ba08fa35b24c"));
    QVERIFY(entry);
    QCOMPARE(entry->type, QStringLiteral("timetrack.entry"));
    QCOMPARE(model.taskOfItem(entry->id), model.task(timetrackTask));

    // Every reference in the fixture layouts refers to something.
    const auto checkCells = [&model](const LayoutNode &node, const auto &self) -> void {
        if (node.kind == LayoutNode::Kind::Cell) {
            if (node.ref)
                QVERIFY2(model.refExists(*node.ref), qPrintable(node.ref->item + node.ref->task));
            return;
        }
        for (const LayoutNode &child : node.children)
            self(child, self);
    };
    for (const RootDashboard *dashboard : model.rootDashboards())
        checkCells(dashboard->layout, checkCells);
}

void TestStoreLoad::reportsProblems()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString root = dir.path();

    writeFile(root, QStringLiteral("groups/g1.json"), R"({"schemaVersion": 1, "id": "g1", "name": "Good"})");
    writeFile(root, QStringLiteral("groups/broken.json"), R"({"schemaVersion": 1, "id": )");
    writeFile(root, QStringLiteral("groups/array.json"), R"([1, 2])");
    writeFile(root, QStringLiteral("groups/v2.json"), R"({"schemaVersion": 2, "id": "v2"})");
    writeFile(root, QStringLiteral("groups/unversioned.json"), R"({"id": "unversioned"})");
    writeFile(root, QStringLiteral("groups/noid.json"), R"({"schemaVersion": 1, "name": "No id"})");
    writeFile(root, QStringLiteral("groups/renamed.json"), R"({"schemaVersion": 1, "id": "other"})");
    writeFile(root, QStringLiteral("groups/notes.txt"), "not a data file");
    writeFile(root, QStringLiteral("projects/p1.json"), R"({"schemaVersion": 1, "id": "p1", "groupId": "g1"})");
    writeFile(root, QStringLiteral("projects/p2.json"), R"({"schemaVersion": 1, "id": "p2", "groupId": "gone"})");
    writeFile(root, QStringLiteral("tasks/t1.json"), R"({"schemaVersion": 1, "id": "t1", "projectId": "p1"})");
    writeFile(root, QStringLiteral("tasks/t2.json"), R"({"schemaVersion": 1, "id": "t2", "projectId": "p2"})");
    // No dashboards folder: that is not a problem.

    Model model;
    const std::vector<LoadProblem> problems = store::load(root, model);

    QStringList files;
    for (const LoadProblem &problem : problems) {
        files << problem.file;
        QVERIFY2(!problem.message.isEmpty(), qPrintable(problem.file));
    }
    const QStringList expected = {
        QStringLiteral("groups/array.json"),
        QStringLiteral("groups/broken.json"),
        QStringLiteral("groups/noid.json"),
        QStringLiteral("groups/renamed.json"),
        QStringLiteral("groups/unversioned.json"),
        QStringLiteral("groups/v2.json"),
        QStringLiteral("projects/p2.json"),
        QStringLiteral("tasks/t2.json"),
    };
    QCOMPARE(files, expected);
    QVERIFY(problems[6].message.contains(QStringLiteral("gone")));
    QVERIFY(problems[7].message.contains(QStringLiteral("p2")));

    QCOMPARE(model.groups().size(), std::size_t(1));
    QVERIFY(model.group(QStringLiteral("g1")));
    QVERIFY(model.project(QStringLiteral("p1")));
    QCOMPARE(model.project(QStringLiteral("p2")), nullptr);
    QVERIFY(model.task(QStringLiteral("t1")));
    QCOMPARE(model.task(QStringLiteral("t2")), nullptr);
    QVERIFY(model.rootDashboards().empty());
}

void TestStoreLoad::emptyFolder()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Model model;
    QSignalSpy spy(&model, &Model::modelReset);
    QVERIFY(store::load(dir.path(), model).empty());
    QCOMPARE(spy.count(), 1);
    QVERIFY(model.groups().empty());
}

void TestStoreLoad::replacesModel()
{
    Model model;
    QVERIFY(store::load(QStringLiteral(MIRANTE_FIXTURES_DIR), model).empty());
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(store::load(dir.path(), model).empty());
    QVERIFY(model.groups().empty());
    QVERIFY(model.rootDashboards().empty());
}

// workspace.mirante lists the profiles. Entries that cannot be used and a
// repeated id are skipped. Default's entry holds its data, without a name.
void TestStoreLoad::profiles()
{
    Model model;
    QVERIFY(store::load(QStringLiteral(MIRANTE_FIXTURES_DIR), model).empty());
    QCOMPARE(model.profiles().size(), std::size_t(2));
    QCOMPARE(model.profiles()[0].id, kDefaultProfile);
    QVERIFY(model.profiles()[0].moduleSettings.contains(QStringLiteral("gitlab")));
    QCOMPARE(model.profiles()[1].name, QStringLiteral("Work"));
    QVERIFY(model.profiles()[1].moduleSettings.contains(QStringLiteral("jira")));
    QCOMPARE(model.defaultProfile(), kDefaultProfile);

    // A file from before decision 0057, without defaultProfile: Default
    // comes first and is the default, and a stored name for it is not used.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    writeFile(dir.path(), QStringLiteral("workspace.mirante"), R"({ "schemaVersion": 1, "name": "W",
        "profiles": [ { "id": "b", "name": "Bank" }, { "name": "No id" }, 3,
                      { "id": "default", "name": "Not the Default", "lock": true },
                      { "id": "a", "name": "Admin", "color": 1 }, { "id": "b", "name": "Repeat" } ] })");
    QVERIFY(store::load(dir.path(), model).empty());
    QCOMPARE(model.profiles().size(), std::size_t(3));
    QCOMPARE(model.profiles()[0].id, kDefaultProfile);
    QCOMPARE(model.profiles()[0].name, QStringLiteral("Default"));
    QVERIFY(model.profiles()[0].extra.value(QStringLiteral("lock")).toBool());
    QCOMPARE(model.defaultProfile(), kDefaultProfile);
    QCOMPARE(model.profiles()[1].id, QStringLiteral("b"));
    QCOMPARE(model.profiles()[1].name, QStringLiteral("Bank"));
    QCOMPARE(model.profiles()[2].id, QStringLiteral("a"));
    QCOMPARE(model.profiles()[2].extra.value(QStringLiteral("color")).toInt(), 1);

    // The file holds them in their order, with their names and unknown keys,
    // and names the default.
    const std::optional<QJsonObject> json = store::fileFor(model, QStringLiteral("workspace.mirante"));
    QVERIFY(json);
    const QJsonArray profiles = json->value(QStringLiteral("profiles")).toArray();
    QCOMPARE(profiles.size(), 3);
    QCOMPARE(profiles.at(0).toObject(), QJsonObject({{QStringLiteral("id"), QStringLiteral("default")},
                                                     {QStringLiteral("name"), QStringLiteral("Default")},
                                                     {QStringLiteral("lock"), true}}));
    QCOMPARE(profiles.at(2).toObject().value(QStringLiteral("color")).toInt(), 1);
    QCOMPARE(json->value(QStringLiteral("defaultProfile")).toString(), kDefaultProfile);

    // A file from this version keeps the order and names as stored, and its
    // default.
    writeFile(dir.path(), QStringLiteral("workspace.mirante"), R"({ "schemaVersion": 1, "name": "W",
        "profiles": [ { "id": "b", "name": "Bank" }, { "id": "default", "name": "Personal" } ],
        "defaultProfile": "b" })");
    QVERIFY(store::load(dir.path(), model).empty());
    QCOMPARE(model.profiles().size(), std::size_t(2));
    QCOMPARE(model.profiles()[1].name, QStringLiteral("Personal"));
    QCOMPARE(model.defaultProfile(), QStringLiteral("b"));
    QCOMPARE(store::fileFor(model, QStringLiteral("workspace.mirante"))->value(QStringLiteral("defaultProfile")),
             QJsonValue(QStringLiteral("b")));

    // Without Default, when another profile is the default.
    writeFile(dir.path(), QStringLiteral("workspace.mirante"), R"({ "schemaVersion": 1, "name": "W",
        "profiles": [ { "id": "b", "name": "Bank" } ], "defaultProfile": "b" })");
    QVERIFY(store::load(dir.path(), model).empty());
    QCOMPARE(model.profiles().size(), std::size_t(1));
    QVERIFY(!model.profile(kDefaultProfile));

    // A default that is not listed falls back on the first by name. An empty
    // list gets Default.
    writeFile(dir.path(), QStringLiteral("workspace.mirante"), R"({ "schemaVersion": 1, "name": "W",
        "profiles": [ { "id": "w", "name": "Work" }, { "id": "b", "name": "Bank" } ], "defaultProfile": "gone" })");
    QVERIFY(store::load(dir.path(), model).empty());
    QCOMPARE(model.defaultProfile(), QStringLiteral("b"));
    writeFile(dir.path(), QStringLiteral("workspace.mirante"), R"({ "schemaVersion": 1, "name": "W",
        "profiles": [], "defaultProfile": "gone" })");
    QVERIFY(store::load(dir.path(), model).empty());
    QCOMPARE(model.profiles().size(), std::size_t(1));
    QCOMPARE(model.defaultProfile(), kDefaultProfile);
    QCOMPARE(model.profiles()[0].name, QStringLiteral("Default"));
}

// Before decision 0050 the list was "webProfiles". It is read when there is
// no "profiles", and written as "profiles".
void TestStoreLoad::oldWebProfiles()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    writeFile(dir.path(), QStringLiteral("workspace.mirante"), R"({ "schemaVersion": 1, "name": "W",
        "webProfiles": [ { "id": "b", "name": "Bank" }, { "id": "default", "name": "Skipped" } ] })");
    Model model;
    QVERIFY(store::load(dir.path(), model).empty());
    QCOMPARE(model.profiles().size(), std::size_t(2));
    QCOMPARE(model.profiles()[1].name, QStringLiteral("Bank"));
    const std::optional<QJsonObject> json = store::fileFor(model, QStringLiteral("workspace.mirante"));
    QVERIFY(json);
    QVERIFY(!json->contains(QStringLiteral("webProfiles")));
    QCOMPARE(json->value(QStringLiteral("profiles")).toArray().size(), 2);

    // A file with both, from an older version that kept the new key, uses
    // "profiles".
    writeFile(dir.path(), QStringLiteral("workspace.mirante"), R"({ "schemaVersion": 1, "name": "W",
        "profiles": [ { "id": "n", "name": "New" } ], "webProfiles": [ { "id": "o", "name": "Old" } ] })");
    QVERIFY(store::load(dir.path(), model).empty());
    QVERIFY(model.profile(QStringLiteral("n")));
    QVERIFY(!model.profile(QStringLiteral("o")));
}

// Each profile holds module settings by module and screen. Modules and keys
// this build does not know are kept. Settings at the top of the file, from
// before decision 0050, move to Default.
void TestStoreLoad::moduleSettings()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    writeFile(dir.path(), QStringLiteral("workspace.mirante"), R"({ "schemaVersion": 1, "name": "W",
        "profiles": [ { "id": "w", "name": "Work", "moduleSettings": {
            "gitlab": { "general": { "projects": [ "acme/backend" ], "future": 1 }, "later": { "x": true } },
            "missing": { "general": { "y": "z" } } } } ] })");
    Model model;
    QVERIFY(store::load(dir.path(), model).empty());
    const QString work = QStringLiteral("w");
    QCOMPARE(model.moduleSettings(work, QStringLiteral("gitlab"), QStringLiteral("general"))
                 .value(QStringLiteral("projects"))
                 .toArray(),
             QJsonArray{QStringLiteral("acme/backend")});
    QCOMPARE(model.moduleSettings(work, QStringLiteral("missing"), QStringLiteral("general"))
                 .value(QStringLiteral("y"))
                 .toString(),
             QStringLiteral("z"));
    QVERIFY(model.moduleSettings(work, QStringLiteral("gitlab"), QStringLiteral("none")).isEmpty());
    QVERIFY(model.moduleSettings(work, QStringLiteral("none"), QStringLiteral("general")).isEmpty());
    QVERIFY(model.moduleSettings(kDefaultProfile, QStringLiteral("gitlab"), QStringLiteral("general")).isEmpty());

    // Written back unchanged, after Default, which a file from before
    // decision 0057 gets first.
    const std::optional<QJsonObject> json = store::fileFor(model, QStringLiteral("workspace.mirante"));
    QVERIFY(json);
    QCOMPARE(json->value(QStringLiteral("profiles")).toArray().at(1).toObject().value(QStringLiteral("moduleSettings")),
             QJsonValue(model.profile(work)->moduleSettings));
    QVERIFY(!json->contains(QStringLiteral("moduleSettings")));

    // The old place, at the top of the file.
    writeFile(dir.path(), QStringLiteral("workspace.mirante"), R"({ "schemaVersion": 1, "name": "W",
        "profiles": [ { "id": "w", "name": "Work" } ],
        "moduleSettings": { "gitlab": { "general": { "projects": [ "acme/web" ] } } } })");
    QVERIFY(store::load(dir.path(), model).empty());
    QCOMPARE(model.moduleSettings(kDefaultProfile, QStringLiteral("gitlab"), QStringLiteral("general"))
                 .value(QStringLiteral("projects"))
                 .toArray(),
             QJsonArray{QStringLiteral("acme/web")});
    QVERIFY(model.moduleSettings(work, QStringLiteral("gitlab"), QStringLiteral("general")).isEmpty());
    const std::optional<QJsonObject> moved = store::fileFor(model, QStringLiteral("workspace.mirante"));
    QVERIFY(moved);
    QVERIFY(!moved->contains(QStringLiteral("moduleSettings")));
    QCOMPARE(moved->value(QStringLiteral("profiles")).toArray().at(0).toObject().value(QStringLiteral("id")),
             QJsonValue(QStringLiteral("default")));
}

// workspace.mirante holds the workspace's name. Saving it keeps the file's other keys.
void TestStoreLoad::workspaceName()
{
    Model model;
    QVERIFY(store::load(QStringLiteral(MIRANTE_FIXTURES_DIR), model).empty());
    QCOMPARE(model.workspaceName(), QStringLiteral("Sample data"));

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(store::load(dir.path(), model).empty());
    QVERIFY(model.workspaceName().isEmpty());   // no workspace.mirante: no name

    writeFile(dir.path(), QStringLiteral("workspace.mirante"), R"({ "schemaVersion": 1, "futureField": 7 })");
    QString error;
    QVERIFY2(store::saveWorkspaceName(dir.path(), QStringLiteral("Work"), &error), qPrintable(error));
    QVERIFY(store::load(dir.path(), model).empty());
    QCOMPARE(model.workspaceName(), QStringLiteral("Work"));
    QFile file(dir.filePath(QStringLiteral("workspace.mirante")));
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QJsonObject json = QJsonDocument::fromJson(file.readAll()).object();
    QCOMPARE(json.value(QStringLiteral("futureField")).toInt(), 7);
    QCOMPARE(json.value(QStringLiteral("schemaVersion")).toInt(), 1);

    // A workspace.mirante of another schemaVersion is reported.
    writeFile(dir.path(), QStringLiteral("workspace.mirante"), R"({ "schemaVersion": 2, "name": "X" })");
    const auto problems = store::load(dir.path(), model);
    QCOMPARE(problems.size(), std::size_t(1));
    QCOMPARE(problems.front().file, QStringLiteral("workspace.mirante"));
}

QTEST_GUILESS_MAIN(TestStoreLoad)
#include "tst_store_load.moc"
