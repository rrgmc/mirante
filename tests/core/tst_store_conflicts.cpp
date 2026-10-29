// Tests the Store's handling of conflict copies made by sync tools: they are
// listed and not loaded, keepCurrent deletes the copy, and useCopy puts the
// copy's content in place of the file and reloads it.
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

QByteArray taskBytes(const Task &task)
{
    QJsonObject json = task.toJson();
    json.insert(QStringLiteral("schemaVersion"), 1);
    return store::toBytes(json);
}

} // namespace

class TestStoreConflicts : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void detectsCopies();
    void detectsNewCopy();
    void keepCurrent();
    void useCopy();
    void useCopyOverPendingEdit();
    void workspaceCopy();

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    Task m_task;
    QString m_taskFile;
    QString m_copyFile;
    QString path(const QString &relPath) const { return m_dir->filePath(relPath); }
};

void TestStoreConflicts::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    QVERIFY(store::createWorkspace(m_dir->path(), QStringLiteral("Test")));
    writeFile(path(QStringLiteral("groups/g1.json")), R"({"schemaVersion":1,"id":"g1","name":"G"})");
    writeFile(path(QStringLiteral("projects/p1.json")), R"({"schemaVersion":1,"id":"p1","groupId":"g1","name":"P"})");

    m_task = edits::newTask(QStringLiteral("p1"), QStringLiteral("Current"));
    m_taskFile = QStringLiteral("tasks/") + m_task.id + QStringLiteral(".json");
    m_copyFile = QStringLiteral("tasks/") + m_task.id + QStringLiteral(".sync-conflict-20261028-101500-ABCDEFG.json");
    writeFile(path(m_taskFile), taskBytes(m_task));
}

void TestStoreConflicts::cleanup()
{
    m_dir.reset();
}

void TestStoreConflicts::detectsCopies()
{
    Task other = m_task;
    other.title = QStringLiteral("Copy");
    writeFile(path(m_copyFile), taskBytes(other));

    Model model;
    Store store(model);
    QSignalSpy spy(&store, &Store::conflictsChanged);
    // The copy is neither loaded nor reported as a problem.
    QVERIFY(store.open(m_dir->path()).empty());
    QCOMPARE(model.task(m_task.id)->title, QStringLiteral("Current"));

    QCOMPARE(spy.count(), 1);
    const std::vector<Store::Conflict> conflicts = store.conflicts();
    QCOMPARE(conflicts.size(), std::size_t(1));
    QCOMPARE(conflicts.front().file, m_taskFile);
    QCOMPARE(conflicts.front().copy, m_copyFile);

    // Closing clears the list.
    store.close();
    QVERIFY(store.conflicts().empty());
    QCOMPARE(spy.count(), 2);
}

void TestStoreConflicts::detectsNewCopy()
{
    Model model;
    Store store(model);
    QVERIFY(store.open(m_dir->path()).empty());
    QVERIFY(store.conflicts().empty());
    QSignalSpy spy(&store, &Store::conflictsChanged);

    // A Dropbox style copy appears while the folder is open.
    const QString copy = QStringLiteral("tasks/") + m_task.id + QStringLiteral(" (Laptop's conflicted copy 2026-10-28).json");
    writeFile(path(copy), taskBytes(m_task));
    QTRY_COMPARE_WITH_TIMEOUT(store.conflicts().size(), std::size_t(1), kTimeout);
    QVERIFY(spy.count() >= 1);
    QCOMPARE(store.conflicts().front().copy, copy);
    QCOMPARE(store.conflicts().front().file, m_taskFile);
    QVERIFY(store.problems().empty());
}

void TestStoreConflicts::keepCurrent()
{
    Task other = m_task;
    other.title = QStringLiteral("Copy");
    writeFile(path(m_copyFile), taskBytes(other));
    const QByteArray current = readFile(path(m_taskFile));

    Model model;
    Store store(model);
    QVERIFY(store.open(m_dir->path()).empty());
    QCOMPARE(store.conflicts().size(), std::size_t(1));
    QSignalSpy spy(&store, &Store::conflictsChanged);

    store.keepCurrent(store.conflicts().front());
    QVERIFY(!QFile::exists(path(m_copyFile)));
    QVERIFY(store.conflicts().empty());
    QCOMPARE(spy.count(), 1);
    QCOMPARE(readFile(path(m_taskFile)), current);
    QCOMPARE(model.task(m_task.id)->title, QStringLiteral("Current"));

    // The watcher's later scan agrees and changes nothing.
    QTest::qWait(1000);
    QVERIFY(store.conflicts().empty());
    QCOMPARE(spy.count(), 1);
}

void TestStoreConflicts::useCopy()
{
    Task other = m_task;
    other.title = QStringLiteral("Copy");
    const QByteArray copyBytes = taskBytes(other);
    writeFile(path(m_copyFile), copyBytes);

    Model model;
    Store store(model);
    QVERIFY(store.open(m_dir->path()).empty());
    QSignalSpy changed(&model, &Model::taskChanged);

    store.useCopy(store.conflicts().front());
    QCOMPARE(readFile(path(m_taskFile)), copyBytes);
    QVERIFY(!QFile::exists(path(m_copyFile)));
    QVERIFY(store.conflicts().empty());
    // Reloaded at once.
    QCOMPARE(model.task(m_task.id)->title, QStringLiteral("Copy"));
    QCOMPARE(changed.count(), 1);

    QTest::qWait(1000);
    QCOMPARE(model.task(m_task.id)->title, QStringLiteral("Copy"));
    QVERIFY(store.problems().empty());
    QVERIFY(store.conflicts().empty());
}

// A local edit still waiting to be written does not land on the copy's content.
void TestStoreConflicts::useCopyOverPendingEdit()
{
    Task other = m_task;
    other.title = QStringLiteral("Copy");
    const QByteArray copyBytes = taskBytes(other);
    writeFile(path(m_copyFile), copyBytes);

    Model model;
    Store store(model);
    QVERIFY(store.open(m_dir->path()).empty());
    Task local = *model.task(m_task.id);
    local.title = QStringLiteral("Local");
    model.setTask(local);

    store.useCopy(store.conflicts().front());
    QCOMPARE(model.task(m_task.id)->title, QStringLiteral("Copy"));
    QTest::qWait(1000);
    store.flush();
    QCOMPARE(readFile(path(m_taskFile)), copyBytes);
    QCOMPARE(model.task(m_task.id)->title, QStringLiteral("Copy"));
}

void TestStoreConflicts::workspaceCopy()
{
    const QString copy = QStringLiteral("workspace.sync-conflict-20261028-101500-ABCDEFG.mirante");
    writeFile(path(copy), R"({"schemaVersion":1,"name":"Other name"})");

    Model model;
    Store store(model);
    QVERIFY(store.open(m_dir->path()).empty());
    QCOMPARE(model.workspaceName(), QStringLiteral("Test"));
    QCOMPARE(store.conflicts().size(), std::size_t(1));
    QCOMPARE(store.conflicts().front().file, QStringLiteral("workspace.mirante"));

    store.useCopy(store.conflicts().front());
    QCOMPARE(model.workspaceName(), QStringLiteral("Other name"));
    QVERIFY(!QFile::exists(path(copy)));
    QVERIFY(store.conflicts().empty());
}

QTEST_GUILESS_MAIN(TestStoreConflicts)
#include "tst_store_conflicts.moc"
