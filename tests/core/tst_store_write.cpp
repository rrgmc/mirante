// Tests that the Store writes the model's changes: each kind of edit writes
// the right file, removals delete files, the sample data is never written, and
// image files are deleted only when a local edit drops the last reference.
#include "core/Edits.h"
#include "core/Model.h"
#include "core/Store.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace mirante;

namespace {

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

// The top-level keys of a pretty-printed file, in the order written.
QStringList topLevelKeys(const QByteArray &bytes)
{
    QStringList keys;
    static const QRegularExpression re(QStringLiteral("^    \"([^\"]+)\":"), QRegularExpression::MultilineOption);
    auto it = re.globalMatch(QString::fromUtf8(bytes));
    while (it.hasNext())
        keys << it.next().captured(1);
    return keys;
}

Group makeGroup(const QString &id, const QString &name)
{
    Group group;
    group.id = id;
    group.name = name;
    return group;
}

Project makeProject(const QString &id, const QString &groupId)
{
    Project project;
    project.id = id;
    project.groupId = groupId;
    project.name = QStringLiteral("Project ") + id;
    return project;
}

Item imageItem(const QString &id, const QString &file)
{
    Item item;
    item.id = id;
    item.type = QStringLiteral("image");
    item.name = QStringLiteral("Image");
    item.config = QJsonObject{{QStringLiteral("file"), file}};
    return item;
}

} // namespace

class TestStoreWrite : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void writesEachKind();
    void keepsWorkspaceKeys();
    void removalsDeleteFiles();
    void trashSurvivesReload();
    void readOnly();
    void imageFiles();
    void saveFile();
    void closeFlushes();

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    QString path(const QString &relPath) const { return m_dir->filePath(relPath); }
    // The file on disk, parsed.
    QJsonObject json(const QString &relPath) const { return QJsonDocument::fromJson(readFile(path(relPath))).object(); }
};

void TestStoreWrite::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    QVERIFY(store::createWorkspace(m_dir->path(), QStringLiteral("Test")));
}

void TestStoreWrite::cleanup()
{
    m_dir.reset();
}

void TestStoreWrite::writesEachKind()
{
    Model model;
    Store store(model);
    QVERIFY(store.open(m_dir->path()).empty());
    QVERIFY(!store.readOnly());
    QCOMPARE(store.dir(), m_dir->path());

    model.setGroup(makeGroup(QStringLiteral("g1"), QStringLiteral("Work")));
    model.setProject(makeProject(QStringLiteral("p1"), QStringLiteral("g1")));
    Task task = edits::newTask(QStringLiteral("p1"), QStringLiteral("Fix login"));
    model.setTask(task);
    RootDashboard dashboard;
    dashboard.id = QStringLiteral("d1");
    dashboard.name = QStringLiteral("Morning");
    model.setRootDashboard(dashboard);
    model.setWorkspaceName(QStringLiteral("Renamed"));

    // Nothing before the debounce or a flush.
    QVERIFY(!QFile::exists(path(QStringLiteral("groups/g1.json"))));
    store.flush();

    const QStringList files = {
        QStringLiteral("groups/g1.json"),
        QStringLiteral("projects/p1.json"),
        QStringLiteral("tasks/") + task.id + QStringLiteral(".json"),
        QStringLiteral("dashboards/d1.json"),
        QStringLiteral("workspace.mirante"),
    };
    for (const QString &file : files) {
        const QByteArray bytes = readFile(path(file));
        QVERIFY2(!bytes.isEmpty(), qPrintable(file));
        const QJsonObject parsed = QJsonDocument::fromJson(bytes).object();
        QCOMPARE(parsed.value(QStringLiteral("schemaVersion")).toInt(), 1);
        const QStringList keys = topLevelKeys(bytes);
        QVERIFY2(!keys.isEmpty(), qPrintable(file));
        QStringList sorted = keys;
        sorted.sort();
        QCOMPARE(keys, sorted);
        // Exactly what fileFor() gives, in the format of toBytes().
        const std::optional<QJsonObject> expected = store::fileFor(model, file);
        QVERIFY2(expected.has_value(), qPrintable(file));
        QCOMPARE(bytes, store::toBytes(*expected));
    }
    QCOMPARE(json(QStringLiteral("groups/g1.json")).value(QStringLiteral("name")).toString(), QStringLiteral("Work"));
    QCOMPARE(json(QStringLiteral("workspace.mirante")).value(QStringLiteral("name")).toString(),
             QStringLiteral("Renamed"));

    // A change rewrites the file.
    model.setGroup(makeGroup(QStringLiteral("g1"), QStringLiteral("Home")));
    store.flush();
    QCOMPARE(json(QStringLiteral("groups/g1.json")).value(QStringLiteral("name")).toString(), QStringLiteral("Home"));

    // The debounce writes without a flush too.
    model.setGroup(makeGroup(QStringLiteral("g1"), QStringLiteral("Later")));
    QTRY_COMPARE_WITH_TIMEOUT(json(QStringLiteral("groups/g1.json")).value(QStringLiteral("name")).toString(),
                              QStringLiteral("Later"), 5000);
    QVERIFY(store.problems().empty());
}

void TestStoreWrite::keepsWorkspaceKeys()
{
    writeFile(path(QStringLiteral("workspace.mirante")),
              R"({ "schemaVersion": 1, "name": "Old", "futureField": 7 })");
    Model model;
    Store store(model);
    QVERIFY(store.open(m_dir->path()).empty());
    QCOMPARE(model.workspaceName(), QStringLiteral("Old"));

    model.setWorkspaceName(QStringLiteral("New"));
    store.flush();
    const QJsonObject written = json(QStringLiteral("workspace.mirante"));
    QCOMPARE(written.value(QStringLiteral("name")).toString(), QStringLiteral("New"));
    QCOMPARE(written.value(QStringLiteral("futureField")).toInt(), 7);
    QCOMPARE(written.value(QStringLiteral("schemaVersion")).toInt(), 1);

    // An empty name keeps the file, so the folder stays a workspace.
    model.setWorkspaceName(QString());
    store.flush();
    QVERIFY(store::isWorkspace(m_dir->path()));
    QCOMPARE(json(QStringLiteral("workspace.mirante")).value(QStringLiteral("futureField")).toInt(), 7);
}

void TestStoreWrite::removalsDeleteFiles()
{
    Model model;
    Store store(model);
    QVERIFY(store.open(m_dir->path()).empty());

    model.setGroup(makeGroup(QStringLiteral("g1"), QStringLiteral("Work")));
    model.setProject(makeProject(QStringLiteral("p1"), QStringLiteral("g1")));
    const Task t1 = edits::newTask(QStringLiteral("p1"), QStringLiteral("One"));
    const Task t2 = edits::newTask(QStringLiteral("p1"), QStringLiteral("Two"));
    model.setTask(t1);
    model.setTask(t2);
    RootDashboard dashboard;
    dashboard.id = QStringLiteral("d1");
    model.setRootDashboard(dashboard);
    store.flush();
    const QString t1File = path(QStringLiteral("tasks/") + t1.id + QStringLiteral(".json"));
    const QString t2File = path(QStringLiteral("tasks/") + t2.id + QStringLiteral(".json"));
    QVERIFY(QFile::exists(t1File));
    QVERIFY(QFile::exists(t2File));

    model.removeTask(t1.id);
    model.removeRootDashboard(QStringLiteral("d1"));
    store.flush();
    QVERIFY(!QFile::exists(t1File));
    QVERIFY(QFile::exists(t2File));
    QVERIFY(!QFile::exists(path(QStringLiteral("dashboards/d1.json"))));

    // A group takes its projects and their tasks with it.
    model.removeGroup(QStringLiteral("g1"));
    store.flush();
    QVERIFY(!QFile::exists(path(QStringLiteral("groups/g1.json"))));
    QVERIFY(!QFile::exists(path(QStringLiteral("projects/p1.json"))));
    QVERIFY(!QFile::exists(t2File));
    QVERIFY(store.problems().empty());
}

// trashedAt is written to the files and read back, so the trash syncs.
void TestStoreWrite::trashSurvivesReload()
{
    const QString when = QStringLiteral("2026-10-29T10:00:00Z");
    Task task = edits::newTask(QStringLiteral("p1"), QStringLiteral("One"));
    {
        Model model;
        Store store(model);
        QVERIFY(store.open(m_dir->path()).empty());
        model.setGroup(makeGroup(QStringLiteral("g1"), QStringLiteral("Work")));
        Project project = makeProject(QStringLiteral("p1"), QStringLiteral("g1"));
        project.trashedAt = when;
        model.setProject(project);
        task.trashedAt = when;
        model.setTask(task);
        store.flush();
    }
    Model model;
    QVERIFY(store::load(m_dir->path(), model).empty());
    QVERIFY(model.project(QStringLiteral("p1")));
    QCOMPARE(model.project(QStringLiteral("p1"))->trashedAt, when);
    QVERIFY(model.task(task.id));
    QCOMPARE(model.task(task.id)->trashedAt, when);
    QVERIFY(model.taskInTrash(task.id));
}

void TestStoreWrite::readOnly()
{
    Model model;
    Store store(model);
    QSignalSpy problems(&store, &Store::problemsChanged);
    store.open(QStringLiteral(":/no-such-folder"));
    QVERIFY(store.readOnly());
    QCOMPARE(problems.count(), 1);

    // Edits change the model only.
    model.setGroup(makeGroup(QStringLiteral("g1"), QStringLiteral("Work")));
    model.setWorkspaceName(QStringLiteral("Changed"));
    store.flush();
    QVERIFY(model.group(QStringLiteral("g1")));
    QVERIFY(store.problems().empty());

    QString error;
    QVERIFY(store.saveFile("data", QStringLiteral("png"), &error).isEmpty());
    QVERIFY(!error.isEmpty());

    // A writable folder opened after it is written to, and the read-only one
    // never was.
    QVERIFY(store.open(m_dir->path()).empty());
    QVERIFY(!store.readOnly());
    QVERIFY(model.groups().empty());
    model.setGroup(makeGroup(QStringLiteral("g2"), QStringLiteral("Home")));
    store.flush();
    QVERIFY(QFile::exists(path(QStringLiteral("groups/g2.json"))));
    QVERIFY(!QFile::exists(path(QStringLiteral("groups/g1.json"))));
}

void TestStoreWrite::imageFiles()
{
    // Files present before opening: one used by two tasks, one never referred to.
    writeFile(path(QStringLiteral("files/shared.png")), "shared");
    writeFile(path(QStringLiteral("files/single.png")), "single");
    writeFile(path(QStringLiteral("files/unreferenced.png")), "unreferenced");

    Model model;
    Store store(model);
    QVERIFY(store.open(m_dir->path()).empty());
    model.setGroup(makeGroup(QStringLiteral("g1"), QStringLiteral("Work")));
    model.setProject(makeProject(QStringLiteral("p1"), QStringLiteral("g1")));
    Task t1 = edits::addItem(edits::newTask(QStringLiteral("p1"), QStringLiteral("One")),
                             imageItem(QStringLiteral("i1"), QStringLiteral("shared.png")));
    Task t2 = edits::addItem(edits::newTask(QStringLiteral("p1"), QStringLiteral("Two")),
                             imageItem(QStringLiteral("i2"), QStringLiteral("shared.png")));
    t2 = edits::addItem(t2, imageItem(QStringLiteral("i3"), QStringLiteral("single.png")));
    model.setTask(t1);
    model.setTask(t2);
    store.flush();

    // Another task still refers to shared.png.
    model.setTask(edits::removeItem(t1, QStringLiteral("i1")));
    store.flush();
    QVERIFY(QFile::exists(path(QStringLiteral("files/shared.png"))));

    // Replacing an image drops the old file.
    Item replaced = imageItem(QStringLiteral("i3"), QStringLiteral("other.png"));
    t2 = edits::addItem(t2, replaced);
    model.setTask(t2);
    store.flush();
    QVERIFY(!QFile::exists(path(QStringLiteral("files/single.png"))));

    // Removing the last task that refers to it drops shared.png.
    model.removeTask(t2.id);
    store.flush();
    QVERIFY(!QFile::exists(path(QStringLiteral("files/shared.png"))));

    // A file found unreferenced on disk is never deleted.
    QVERIFY(QFile::exists(path(QStringLiteral("files/unreferenced.png"))));
    model.removeGroup(QStringLiteral("g1"));
    store.flush();
    QVERIFY(QFile::exists(path(QStringLiteral("files/unreferenced.png"))));
    QVERIFY(store.problems().empty());
}

void TestStoreWrite::saveFile()
{
    Model model;
    Store store(model);
    QVERIFY(store.open(m_dir->path()).empty());

    QString error;
    const QString name = store.saveFile("png bytes", QStringLiteral("png"), &error);
    QVERIFY2(!name.isEmpty(), qPrintable(error));
    QVERIFY(QRegularExpression(QStringLiteral("^[0-9a-f-]{36}\\.png$")).match(name).hasMatch());
    // It exists at once.
    QCOMPARE(readFile(store.filePath(name)), QByteArray("png bytes"));
    QCOMPARE(QFileInfo(store.filePath(name)).absoluteFilePath(),
             QFileInfo(path(QStringLiteral("files/") + name)).absoluteFilePath());

    // A leading dot in the extension is accepted.
    const QString other = store.saveFile("x", QStringLiteral(".jpg"));
    QVERIFY(other.endsWith(QStringLiteral(".jpg")));
    QVERIFY(!other.endsWith(QStringLiteral("..jpg")));
    QVERIFY(name != other);
}

void TestStoreWrite::closeFlushes()
{
    Model model;
    Store store(model);
    QVERIFY(store.open(m_dir->path()).empty());
    model.setGroup(makeGroup(QStringLiteral("g1"), QStringLiteral("Work")));
    store.close();
    QVERIFY(QFile::exists(path(QStringLiteral("groups/g1.json"))));
    QVERIFY(store.dir().isEmpty());

    // After closing, edits are not written.
    model.setGroup(makeGroup(QStringLiteral("g2"), QStringLiteral("Home")));
    store.flush();
    QTest::qWait(700);
    QVERIFY(!QFile::exists(path(QStringLiteral("groups/g2.json"))));

    // Destroying an open store flushes it too.
    {
        Store second(model);
        QVERIFY(second.open(m_dir->path()).empty());
        model.setGroup(makeGroup(QStringLiteral("g3"), QStringLiteral("Third")));
    }
    QVERIFY(QFile::exists(path(QStringLiteral("groups/g3.json"))));
}

QTEST_GUILESS_MAIN(TestStoreWrite)
#include "tst_store_write.moc"
