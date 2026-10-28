// Tests store::load(): loading the fixtures, and the list of load problems.
#include "core/Model.h"
#include "core/Store.h"

#include <QDir>
#include <QFile>
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
