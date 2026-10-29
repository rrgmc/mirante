// Tests FolderWatcher: external changes and removals reported after the delay,
// own writes skipped, conflict copies listed apart, temporary files ignored,
// and subfolders that appear later.
#include "core/FileWriter.h"
#include "core/FolderWatcher.h"

#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace mirante;

namespace {

const QString kTaskId = QStringLiteral("0b6f4e4c-2f7e-4d3a-9c41-6f1c2a7d9e01");
const QString kOtherId = QStringLiteral("5d2a1b9e-8c3f-4e6a-b7d0-1a2b3c4d5e6f");

// Generous, because file system events can be slow on a busy machine.
constexpr int kTimeout = 10000;
constexpr int kDelay = 50;

bool writeFile(const QString &path, const QByteArray &content)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(content) == content.size();
}

// Makes the data folder with its subfolders, except those in missing.
void makeFolders(const QTemporaryDir &dir, const QStringList &missing = {})
{
    for (const QString &name : {QStringLiteral("groups"), QStringLiteral("projects"), QStringLiteral("tasks"),
                                QStringLiteral("dashboards"), QStringLiteral("files")}) {
        if (!missing.contains(name))
            QDir(dir.path()).mkdir(name);
    }
}

QStringList flatten(const QSignalSpy &spy)
{
    QStringList all;
    for (const QList<QVariant> &args : spy)
        all += args.at(0).toStringList();
    return all;
}

FolderWatcher::OwnWriteCheck noOwnWrites()
{
    return [](const QString &, const QByteArray &) { return false; };
}

} // namespace

class TestFolderWatcher : public QObject
{
    Q_OBJECT

private slots:
    void startReportsNothing();
    void externalChangeReportedOnce();
    void inPlaceModification();
    void ownWritesSkipped();
    void removedFile();
    void onlyCountedFiles();
    void conflictCopies();
    void conflictCopiesReportedAtStart();
    void temporaryFilesIgnored();
    void newSubfolderPickedUp();
    void stopStopsReporting();
};

void TestFolderWatcher::startReportsNothing()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    makeFolders(dir);
    QVERIFY(writeFile(dir.filePath(QStringLiteral("workspace.mirante")), "{}"));
    QVERIFY(writeFile(dir.filePath(QStringLiteral("tasks/") + kTaskId + QStringLiteral(".json")), "{}"));

    FolderWatcher watcher(dir.path(), noOwnWrites());
    watcher.setDelay(kDelay);
    QSignalSpy changed(&watcher, &FolderWatcher::filesChanged);
    QSignalSpy removed(&watcher, &FolderWatcher::filesRemoved);
    QSignalSpy copies(&watcher, &FolderWatcher::conflictCopiesChanged);
    watcher.start();

    QTest::qWait(300);
    QCOMPARE(changed.count(), 0);
    QCOMPARE(removed.count(), 0);
    QCOMPARE(copies.count(), 0);
}

void TestFolderWatcher::externalChangeReportedOnce()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    makeFolders(dir);
    const QString rel = QStringLiteral("tasks/") + kTaskId + QStringLiteral(".json");

    FolderWatcher watcher(dir.path(), noOwnWrites());
    watcher.setDelay(300);
    QSignalSpy changed(&watcher, &FolderWatcher::filesChanged);
    watcher.start();

    // A QSaveFile write, as another instance or a sync tool would make.
    QSaveFile save(dir.filePath(rel));
    QVERIFY(save.open(QIODevice::WriteOnly));
    save.write("{\"title\":\"one\"}");
    QVERIFY(save.commit());
    QVERIFY(writeFile(dir.filePath(QStringLiteral("workspace.mirante")), "{}"));

    // Not before the delay.
    QTest::qWait(100);
    QCOMPARE(changed.count(), 0);

    QTRY_COMPARE_WITH_TIMEOUT(changed.count(), 1, kTimeout);
    QCOMPARE(changed.at(0).at(0).toStringList(), QStringList({rel, QStringLiteral("workspace.mirante")}));

    // Once only.
    QTest::qWait(600);
    QCOMPARE(changed.count(), 1);
}

void TestFolderWatcher::inPlaceModification()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    makeFolders(dir);
    const QString rel = QStringLiteral("tasks/") + kTaskId + QStringLiteral(".json");
    QVERIFY(writeFile(dir.filePath(rel), "{}"));

    FolderWatcher watcher(dir.path(), noOwnWrites());
    watcher.setDelay(kDelay);
    QSignalSpy changed(&watcher, &FolderWatcher::filesChanged);
    watcher.start();

    // Written in place, without a rename. The size changes, so a coarse
    // modification time does not hide it.
    {
        QFile file(dir.filePath(rel));
        QVERIFY(file.open(QIODevice::ReadWrite));
        QVERIFY(file.seek(file.size()));
        QVERIFY(file.write("\n\n\n") == 3);
    }

    QTRY_COMPARE_WITH_TIMEOUT(changed.count(), 1, kTimeout);
    QCOMPARE(changed.at(0).at(0).toStringList(), QStringList({rel}));
}

void TestFolderWatcher::ownWritesSkipped()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    makeFolders(dir);
    const QString own = QStringLiteral("tasks/") + kTaskId + QStringLiteral(".json");
    const QString other = QStringLiteral("tasks/") + kOtherId + QStringLiteral(".json");

    FileWriter writer(dir.path());
    FolderWatcher watcher(dir.path(), [&writer](const QString &relPath, const QByteArray &content) {
        return writer.isOwnWrite(relPath, content);
    });
    watcher.setDelay(kDelay);
    QSignalSpy changed(&watcher, &FolderWatcher::filesChanged);
    QSignalSpy removed(&watcher, &FolderWatcher::filesRemoved);
    watcher.start();

    writer.schedule(own, QByteArray("{\"title\":\"mine\"}"));
    writer.flush();
    // Another file changed by someone else marks the end of the events.
    QVERIFY(writeFile(dir.filePath(other), "{}"));

    QTRY_VERIFY_WITH_TIMEOUT(flatten(changed).contains(other), kTimeout);
    QTest::qWait(300);
    QVERIFY(!flatten(changed).contains(own));

    // Someone else then changes the same file.
    QVERIFY(writeFile(dir.filePath(own), "{\"title\":\"theirs, longer\"}"));
    QTRY_VERIFY_WITH_TIMEOUT(flatten(changed).contains(own), kTimeout);

    // Own deletions are not reported either.
    writer.schedule(own, std::nullopt);
    writer.flush();
    QVERIFY(QFile::remove(dir.filePath(other)));
    QTRY_VERIFY_WITH_TIMEOUT(flatten(removed).contains(other), kTimeout);
    QTest::qWait(300);
    QVERIFY(!flatten(removed).contains(own));
}

void TestFolderWatcher::removedFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    makeFolders(dir);
    const QString rel = QStringLiteral("projects/") + kTaskId + QStringLiteral(".json");
    QVERIFY(writeFile(dir.filePath(rel), "{}"));

    FolderWatcher watcher(dir.path(), noOwnWrites());
    watcher.setDelay(kDelay);
    QSignalSpy changed(&watcher, &FolderWatcher::filesChanged);
    QSignalSpy removed(&watcher, &FolderWatcher::filesRemoved);
    watcher.start();

    QVERIFY(QFile::remove(dir.filePath(rel)));
    QTRY_COMPARE_WITH_TIMEOUT(removed.count(), 1, kTimeout);
    QCOMPARE(removed.at(0).at(0).toStringList(), QStringList({rel}));
    QCOMPARE(changed.count(), 0);
}

void TestFolderWatcher::onlyCountedFiles()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    makeFolders(dir);

    FolderWatcher watcher(dir.path(), noOwnWrites());
    watcher.setDelay(kDelay);
    QSignalSpy changed(&watcher, &FolderWatcher::filesChanged);
    watcher.start();

    // Not counted: other files at the top level, and files other than *.json
    // in the entity folders.
    QVERIFY(writeFile(dir.filePath(QStringLiteral("notes.txt")), "x"));
    QVERIFY(writeFile(dir.filePath(QStringLiteral("tasks/readme.md")), "x"));
    // Counted: any file in files/.
    const QString image = QStringLiteral("files/") + kTaskId + QStringLiteral(".png");
    QVERIFY(writeFile(dir.filePath(image), "png"));

    QTRY_COMPARE_WITH_TIMEOUT(changed.count(), 1, kTimeout);
    QTest::qWait(300);
    QCOMPARE(flatten(changed), QStringList({image}));
}

void TestFolderWatcher::conflictCopies()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    makeFolders(dir);
    const QString original = QStringLiteral("tasks/") + kTaskId + QStringLiteral(".json");
    QVERIFY(writeFile(dir.filePath(original), "{}"));

    FolderWatcher watcher(dir.path(), noOwnWrites());
    watcher.setDelay(kDelay);
    QSignalSpy changed(&watcher, &FolderWatcher::filesChanged);
    QSignalSpy removed(&watcher, &FolderWatcher::filesRemoved);
    QSignalSpy copies(&watcher, &FolderWatcher::conflictCopiesChanged);
    watcher.start();
    QCOMPARE(copies.count(), 0);

    const QString syncthing =
        QStringLiteral("tasks/") + kTaskId + QStringLiteral(".sync-conflict-20261028-101500-ABCDEFG.json");
    const QString workspace = QStringLiteral("workspace-LAPTOP.mirante");
    QVERIFY(writeFile(dir.filePath(syncthing), "{\"a\":1}"));
    QVERIFY(writeFile(dir.filePath(workspace), "{}"));

    QTRY_VERIFY_WITH_TIMEOUT(!copies.isEmpty()
                                 && copies.last().at(0).toStringList() == QStringList({syncthing, workspace}),
                             kTimeout);
    QCOMPARE(changed.count(), 0);

    // A second copy: the signal carries the full list.
    const QString onedrive = QStringLiteral("tasks/") + kTaskId + QStringLiteral("-LAPTOP.json");
    QVERIFY(writeFile(dir.filePath(onedrive), "{\"b\":2}"));
    QTRY_VERIFY_WITH_TIMEOUT(copies.last().at(0).toStringList() == QStringList({onedrive, syncthing, workspace}),
                             kTimeout);

    // A copy that is resolved.
    const int before = copies.count();
    QVERIFY(QFile::remove(dir.filePath(syncthing)));
    QVERIFY(QFile::remove(dir.filePath(workspace)));
    QTRY_VERIFY_WITH_TIMEOUT(copies.last().at(0).toStringList() == QStringList({onedrive}), kTimeout);
    QVERIFY(copies.count() > before);
    QCOMPARE(changed.count(), 0);
    QCOMPARE(removed.count(), 0);

    // An unrelated change does not repeat an unchanged list.
    const int settled = copies.count();
    QVERIFY(writeFile(dir.filePath(original), "{\"changed\":true}"));
    QTRY_COMPARE_WITH_TIMEOUT(changed.count(), 1, kTimeout);
    QCOMPARE(copies.count(), settled);
}

void TestFolderWatcher::conflictCopiesReportedAtStart()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    makeFolders(dir);
    const QString copy = QStringLiteral("files/") + kTaskId + QStringLiteral(" (conflicted copy).png");
    QVERIFY(writeFile(dir.filePath(copy), "png"));

    FolderWatcher watcher(dir.path(), noOwnWrites());
    QSignalSpy copies(&watcher, &FolderWatcher::conflictCopiesChanged);
    watcher.start();
    QCOMPARE(copies.count(), 1);
    QCOMPARE(copies.at(0).at(0).toStringList(), QStringList({copy}));
}

void TestFolderWatcher::temporaryFilesIgnored()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    makeFolders(dir);

    FolderWatcher watcher(dir.path(), noOwnWrites());
    watcher.setDelay(kDelay);
    QSignalSpy changed(&watcher, &FolderWatcher::filesChanged);
    QSignalSpy removed(&watcher, &FolderWatcher::filesRemoved);
    QSignalSpy copies(&watcher, &FolderWatcher::conflictCopiesChanged);
    watcher.start();

    const QStringList temporaries = {
        // QSaveFile, which originalOf alone would take for a conflict copy.
        QStringLiteral("tasks/") + kTaskId + QStringLiteral(".json.AbCdEf"),
        QStringLiteral("files/") + kTaskId + QStringLiteral(".png.x1Y2z3"),
        QStringLiteral("workspace.mirante.Qw3rTy"),
        // Syncthing.
        QStringLiteral("tasks/.syncthing.") + kTaskId + QStringLiteral(".json.tmp"),
        // Editors and office programs.
        QStringLiteral("tasks/~") + kTaskId + QStringLiteral(".json"),
        QStringLiteral("files/.~lock.") + kTaskId + QStringLiteral(".png#"),
        // Downloads in progress.
        QStringLiteral("files/") + kTaskId + QStringLiteral(".png.partial"),
        QStringLiteral("files/") + kTaskId + QStringLiteral(".png.crdownload"),
        QStringLiteral("files/") + kTaskId + QStringLiteral(".tmp"),
    };
    for (const QString &rel : temporaries)
        QVERIFY2(writeFile(dir.filePath(rel), "partial"), qPrintable(rel));
    // A real change after them, to know the scan happened.
    const QString real = QStringLiteral("tasks/") + kOtherId + QStringLiteral(".json");
    QVERIFY(writeFile(dir.filePath(real), "{}"));

    QTRY_VERIFY_WITH_TIMEOUT(flatten(changed).contains(real), kTimeout);
    QTest::qWait(300);
    QCOMPARE(flatten(changed), QStringList({real}));
    QCOMPARE(copies.count(), 0);

    // Removing them is not reported either.
    for (const QString &rel : temporaries)
        QVERIFY(QFile::remove(dir.filePath(rel)));
    QVERIFY(QFile::remove(dir.filePath(real)));
    QTRY_VERIFY_WITH_TIMEOUT(!removed.isEmpty(), kTimeout);
    QTest::qWait(300);
    QCOMPARE(flatten(removed), QStringList({real}));
}

void TestFolderWatcher::newSubfolderPickedUp()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    makeFolders(dir, {QStringLiteral("dashboards")});

    FolderWatcher watcher(dir.path(), noOwnWrites());
    watcher.setDelay(kDelay);
    QSignalSpy changed(&watcher, &FolderWatcher::filesChanged);
    watcher.start();

    // The folder appears with a file already in it, as a sync tool makes it.
    QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("dashboards")));
    const QString first = QStringLiteral("dashboards/") + kTaskId + QStringLiteral(".json");
    QVERIFY(writeFile(dir.filePath(first), "{}"));
    QTRY_VERIFY_WITH_TIMEOUT(flatten(changed).contains(first), kTimeout);

    // Later changes in it are seen too.
    const QString second = QStringLiteral("dashboards/") + kOtherId + QStringLiteral(".json");
    QVERIFY(writeFile(dir.filePath(second), "{}"));
    QTRY_VERIFY_WITH_TIMEOUT(flatten(changed).contains(second), kTimeout);
}

void TestFolderWatcher::stopStopsReporting()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    makeFolders(dir);

    FolderWatcher watcher(dir.path(), noOwnWrites());
    watcher.setDelay(kDelay);
    QSignalSpy changed(&watcher, &FolderWatcher::filesChanged);
    watcher.start();
    watcher.stop();

    QVERIFY(writeFile(dir.filePath(QStringLiteral("tasks/") + kTaskId + QStringLiteral(".json")), "{}"));
    QTest::qWait(500);
    QCOMPARE(changed.count(), 0);
}

QTEST_GUILESS_MAIN(TestFolderWatcher)
#include "tst_folder_watcher.moc"
