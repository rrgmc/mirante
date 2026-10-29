// Tests that the Store reloads changes made by others: an external change
// updates the model in place, a broken file is reported and the model keeps
// the old entity, a removed file removes the entity, and a reload is never
// written back. Two Stores on one folder see each other's edits.
#include "core/Edits.h"
#include "core/Model.h"
#include "core/Store.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace mirante;

namespace {

// The watcher waits 300 ms of quiet; this leaves room for a slow machine.
constexpr int kTimeout = 10000;

QByteArray readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.readAll();
}

void writeFile(const QString &path, const QByteArray &content)
{
    QVERIFY(QDir().mkpath(QFileInfo(path).path()));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(content), qint64(content.size()));
}

QByteArray groupJson(const QString &id, const QString &name)
{
    // Compact, unlike what the Store writes, so a write-back would show.
    return QStringLiteral(R"({"schemaVersion":1,"id":"%1","name":"%2"})").arg(id, name).toUtf8();
}

QByteArray projectJson(const QString &id, const QString &groupId)
{
    return QStringLiteral(R"({"schemaVersion":1,"id":"%1","groupId":"%2","name":"P"})").arg(id, groupId).toUtf8();
}

QByteArray taskBytes(const Task &task)
{
    QJsonObject json = task.toJson();
    json.insert(QStringLiteral("schemaVersion"), 1);
    return store::toBytes(json);
}

bool hasProblem(const Store &store, const QString &file)
{
    for (const LoadProblem &problem : store.problems()) {
        if (problem.file == file)
            return true;
    }
    return false;
}

QString groupName(const Model &model, const QString &id)
{
    const Group *group = model.group(id);
    return group ? group->name : QString();
}

} // namespace

class TestStoreReload : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void externalChange();
    void brokenFile();
    void newerFile();
    void removedFile();
    void removedGroupWithChildren();
    void parentArrivesLater();
    void workspaceFile();
    void reloadKeepsImageFiles();
    void twoStores();

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    QString path(const QString &relPath) const { return m_dir->filePath(relPath); }
};

void TestStoreReload::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    QVERIFY(store::createWorkspace(m_dir->path(), QStringLiteral("Test")));
    writeFile(path(QStringLiteral("groups/g1.json")), groupJson(QStringLiteral("g1"), QStringLiteral("Work")));
    writeFile(path(QStringLiteral("projects/p1.json")), projectJson(QStringLiteral("p1"), QStringLiteral("g1")));
}

void TestStoreReload::cleanup()
{
    m_dir.reset();
}

void TestStoreReload::externalChange()
{
    Model model;
    Store store(model);
    QVERIFY(store.open(m_dir->path()).empty());
    QSignalSpy changed(&model, &Model::groupChanged);
    QSignalSpy added(&model, &Model::groupAdded);
    QSignalSpy reset(&model, &Model::modelReset);

    const QByteArray content = groupJson(QStringLiteral("g1"), QStringLiteral("Renamed"));
    writeFile(path(QStringLiteral("groups/g1.json")), content);
    QTRY_COMPARE_WITH_TIMEOUT(groupName(model, QStringLiteral("g1")), QStringLiteral("Renamed"), kTimeout);
    QCOMPARE(changed.count(), 1);
    QCOMPARE(changed.first().first().toString(), QStringLiteral("g1"));

    // A new file adds an entity.
    writeFile(path(QStringLiteral("groups/g2.json")), groupJson(QStringLiteral("g2"), QStringLiteral("Home")));
    QTRY_COMPARE_WITH_TIMEOUT(groupName(model, QStringLiteral("g2")), QStringLiteral("Home"), kTimeout);
    QCOMPARE(added.count(), 1);
    QCOMPARE(reset.count(), 0);

    // A reload is not written back: the files keep their compact form.
    store.flush();
    QTest::qWait(700);
    QCOMPARE(readFile(path(QStringLiteral("groups/g1.json"))), content);
    QVERIFY(store.problems().empty());
}

void TestStoreReload::brokenFile()
{
    Model model;
    Store store(model);
    QVERIFY(store.open(m_dir->path()).empty());
    QSignalSpy problems(&store, &Store::problemsChanged);

    writeFile(path(QStringLiteral("groups/g1.json")), R"({"schemaVersion":1,"id":)");
    QTRY_VERIFY_WITH_TIMEOUT(hasProblem(store, QStringLiteral("groups/g1.json")), kTimeout);
    QVERIFY(problems.count() >= 1);
    QCOMPARE(groupName(model, QStringLiteral("g1")), QStringLiteral("Work"));
    QVERIFY(model.project(QStringLiteral("p1")));

    // A file whose name does not match its id is broken too.
    writeFile(path(QStringLiteral("groups/g2.json")), groupJson(QStringLiteral("other"), QStringLiteral("X")));
    QTRY_VERIFY_WITH_TIMEOUT(hasProblem(store, QStringLiteral("groups/g2.json")), kTimeout);
    QCOMPARE(model.group(QStringLiteral("other")), nullptr);

    // Repairing the file clears the problem.
    writeFile(path(QStringLiteral("groups/g1.json")), groupJson(QStringLiteral("g1"), QStringLiteral("Fixed")));
    QTRY_COMPARE_WITH_TIMEOUT(groupName(model, QStringLiteral("g1")), QStringLiteral("Fixed"), kTimeout);
    QVERIFY(!hasProblem(store, QStringLiteral("groups/g1.json")));
}

void TestStoreReload::newerFile()
{
    Model model;
    Store store(model);
    QVERIFY(store.open(m_dir->path()).empty());

    const QByteArray newer = R"({"schemaVersion":2,"id":"g1","name":"From the future"})";
    writeFile(path(QStringLiteral("groups/g1.json")), newer);
    QTRY_VERIFY_WITH_TIMEOUT(hasProblem(store, QStringLiteral("groups/g1.json")), kTimeout);
    QCOMPARE(groupName(model, QStringLiteral("g1")), QStringLiteral("Work"));

    // A local edit changes the model but does not write over the newer file.
    Group group = *model.group(QStringLiteral("g1"));
    group.name = QStringLiteral("Local");
    model.setGroup(group);
    store.flush();
    QCOMPARE(readFile(path(QStringLiteral("groups/g1.json"))), newer);

    // A newer file found on loading is reported too, and its group's project
    // with it.
    const std::vector<LoadProblem> problems = store.open(m_dir->path());
    QCOMPARE(problems.size(), std::size_t(2));
    QCOMPARE(problems[0].file, QStringLiteral("groups/g1.json"));
    QVERIFY(problems[0].message.contains(QStringLiteral("newer")));
    QCOMPARE(problems[1].file, QStringLiteral("projects/p1.json"));
    model.setGroup(group);
    store.flush();
    QCOMPARE(readFile(path(QStringLiteral("groups/g1.json"))), newer);
}

void TestStoreReload::removedFile()
{
    const Task task = edits::newTask(QStringLiteral("p1"), QStringLiteral("Task"));
    const QString taskFile = QStringLiteral("tasks/") + task.id + QStringLiteral(".json");
    writeFile(path(taskFile), taskBytes(task));

    Model model;
    Store store(model);
    QVERIFY(store.open(m_dir->path()).empty());
    QVERIFY(model.task(task.id));
    QSignalSpy removed(&model, &Model::taskRemoved);

    QVERIFY(QFile::remove(path(taskFile)));
    QTRY_COMPARE_WITH_TIMEOUT(model.task(task.id), nullptr, kTimeout);
    QCOMPARE(removed.count(), 1);
    QVERIFY(model.project(QStringLiteral("p1")));
    QVERIFY(store.problems().empty());
}

// Removing only a group's file removes the group and, as load() would, its
// projects, whose files are then reported.
void TestStoreReload::removedGroupWithChildren()
{
    Model model;
    Store store(model);
    QVERIFY(store.open(m_dir->path()).empty());

    QVERIFY(QFile::remove(path(QStringLiteral("groups/g1.json"))));
    QTRY_COMPARE_WITH_TIMEOUT(model.group(QStringLiteral("g1")), nullptr, kTimeout);
    QCOMPARE(model.project(QStringLiteral("p1")), nullptr);
    QVERIFY(hasProblem(store, QStringLiteral("projects/p1.json")));
    // Nothing is deleted: the project's file stays.
    store.flush();
    QVERIFY(QFile::exists(path(QStringLiteral("projects/p1.json"))));

    // The group coming back brings its project back.
    writeFile(path(QStringLiteral("groups/g1.json")), groupJson(QStringLiteral("g1"), QStringLiteral("Back")));
    QTRY_VERIFY_WITH_TIMEOUT(model.project(QStringLiteral("p1")) != nullptr, kTimeout);
    QVERIFY(store.problems().empty());
}

// A sync tool may deliver a child before its parent.
void TestStoreReload::parentArrivesLater()
{
    Model model;
    Store store(model);
    QVERIFY(store.open(m_dir->path()).empty());

    writeFile(path(QStringLiteral("projects/p2.json")), projectJson(QStringLiteral("p2"), QStringLiteral("g2")));
    QTRY_VERIFY_WITH_TIMEOUT(hasProblem(store, QStringLiteral("projects/p2.json")), kTimeout);
    QCOMPARE(model.project(QStringLiteral("p2")), nullptr);

    writeFile(path(QStringLiteral("groups/g2.json")), groupJson(QStringLiteral("g2"), QStringLiteral("Late")));
    QTRY_VERIFY_WITH_TIMEOUT(model.project(QStringLiteral("p2")) != nullptr, kTimeout);
    QVERIFY(store.problems().empty());
}

void TestStoreReload::workspaceFile()
{
    Model model;
    Store store(model);
    QVERIFY(store.open(m_dir->path()).empty());

    writeFile(path(QStringLiteral("workspace.mirante")), R"({"schemaVersion":1,"name":"Other","futureField":3})");
    QTRY_COMPARE_WITH_TIMEOUT(model.workspaceName(), QStringLiteral("Other"), kTimeout);

    // A local rename keeps the key the other device wrote.
    model.setWorkspaceName(QStringLiteral("Mine"));
    store.flush();
    const QJsonObject json = QJsonDocument::fromJson(readFile(path(QStringLiteral("workspace.mirante")))).object();
    QCOMPARE(json.value(QStringLiteral("name")).toString(), QStringLiteral("Mine"));
    QCOMPARE(json.value(QStringLiteral("futureField")).toInt(), 3);
}

// Only local edits delete image files. A reload that drops a reference leaves
// the file to the device that made the edit.
void TestStoreReload::reloadKeepsImageFiles()
{
    writeFile(path(QStringLiteral("files/a.png")), "a");
    Task task = edits::newTask(QStringLiteral("p1"), QStringLiteral("Task"));
    Item item;
    item.id = QStringLiteral("i1");
    item.type = QStringLiteral("image");
    item.config = QJsonObject{{QStringLiteral("file"), QStringLiteral("a.png")}};
    task = edits::addItem(task, item);
    const QString taskFile = QStringLiteral("tasks/") + task.id + QStringLiteral(".json");
    writeFile(path(taskFile), taskBytes(task));

    Model model;
    Store store(model);
    QVERIFY(store.open(m_dir->path()).empty());

    writeFile(path(taskFile), taskBytes(edits::removeItem(task, QStringLiteral("i1"))));
    QTRY_VERIFY_WITH_TIMEOUT(model.task(task.id)->items.empty(), kTimeout);
    store.flush();
    QTest::qWait(700);
    QVERIFY(QFile::exists(path(QStringLiteral("files/a.png"))));
}

void TestStoreReload::twoStores()
{
    Model modelA;
    Model modelB;
    Store storeA(modelA);
    Store storeB(modelB);
    QVERIFY(storeA.open(m_dir->path()).empty());
    QVERIFY(storeB.open(m_dir->path()).empty());

    // An edit in one appears in the other.
    const Task task = edits::newTask(QStringLiteral("p1"), QStringLiteral("From A"));
    modelA.setTask(task);
    storeA.flush();
    QTRY_VERIFY_WITH_TIMEOUT(modelB.task(task.id) != nullptr, kTimeout);
    QCOMPARE(modelB.task(task.id)->title, QStringLiteral("From A"));

    // And back.
    Task edited = *modelB.task(task.id);
    edited.title = QStringLiteral("Edited in B");
    modelB.setTask(edited);
    storeB.flush();
    QTRY_COMPARE_WITH_TIMEOUT(modelA.task(task.id)->title, QStringLiteral("Edited in B"), kTimeout);

    // A deletion too.
    modelA.removeGroup(QStringLiteral("g1"));
    storeA.flush();
    QTRY_COMPARE_WITH_TIMEOUT(modelB.group(QStringLiteral("g1")), nullptr, kTimeout);
    QCOMPARE(modelB.task(task.id), nullptr);

    // Neither wrote back what it reloaded, so the files stay gone.
    QTest::qWait(1000);
    storeA.flush();
    storeB.flush();
    QVERIFY(!QFile::exists(path(QStringLiteral("groups/g1.json"))));
    QVERIFY(!QFile::exists(path(QStringLiteral("tasks/") + task.id + QStringLiteral(".json"))));
    QVERIFY(storeA.problems().empty());
    QTRY_VERIFY_WITH_TIMEOUT(storeB.problems().empty(), kTimeout);
}

QTEST_GUILESS_MAIN(TestStoreReload)
#include "tst_store_reload.moc"
