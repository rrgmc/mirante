// Tests FileWriter: debouncing, replacing and deleting, own writes, and failed
// writes that are retried.
#include "core/FileWriter.h"

#include <QDir>
#include <QFile>
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

bool writeFile(const QString &path, const QByteArray &content)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(content) == content.size();
}

} // namespace

class TestFileWriter : public QObject
{
    Q_OBJECT

private slots:
    void zeroDelayWritesOnNextTurn();
    void debounceRestartsOnSchedule();
    void flushWritesNow();
    void laterScheduleReplacesPending();
    void createsSubfolders();
    void deletes();
    void ownWrites();
    void failedWriteIsRetried();
};

void TestFileWriter::zeroDelayWritesOnNextTurn()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileWriter writer(dir.path());
    writer.setDelay(0);
    QSignalSpy written(&writer, &FileWriter::written);

    writer.schedule(QStringLiteral("a.json"), QByteArray("one"));
    QVERIFY(writer.pending());
    QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("a.json"))));

    QTRY_COMPARE(written.count(), 1);
    QCOMPARE(written.at(0).at(0).toString(), QStringLiteral("a.json"));
    QCOMPARE(readFile(dir.filePath(QStringLiteral("a.json"))), QByteArray("one"));
    QVERIFY(!writer.pending());
}

void TestFileWriter::debounceRestartsOnSchedule()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileWriter writer(dir.path());
    writer.setDelay(300);
    QSignalSpy written(&writer, &FileWriter::written);

    writer.schedule(QStringLiteral("a.json"), QByteArray("one"));
    QTest::qWait(200);
    writer.schedule(QStringLiteral("b.json"), QByteArray("two"));
    QTest::qWait(200);
    // 400 ms after the first schedule, but only 200 ms after the second.
    QCOMPARE(written.count(), 0);
    QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("a.json"))));

    QTRY_COMPARE(written.count(), 2);
    QCOMPARE(readFile(dir.filePath(QStringLiteral("a.json"))), QByteArray("one"));
    QCOMPARE(readFile(dir.filePath(QStringLiteral("b.json"))), QByteArray("two"));
}

void TestFileWriter::flushWritesNow()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileWriter writer(dir.path());
    QSignalSpy written(&writer, &FileWriter::written);

    writer.schedule(QStringLiteral("a.json"), QByteArray("one"));
    writer.flush();
    QCOMPARE(written.count(), 1);
    QVERIFY(!writer.pending());
    QCOMPARE(readFile(dir.filePath(QStringLiteral("a.json"))), QByteArray("one"));
}

void TestFileWriter::laterScheduleReplacesPending()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileWriter writer(dir.path());
    QSignalSpy written(&writer, &FileWriter::written);

    writer.schedule(QStringLiteral("a.json"), QByteArray("one"));
    writer.schedule(QStringLiteral("a.json"), QByteArray("two"));
    writer.flush();
    QCOMPARE(written.count(), 1);
    QCOMPARE(readFile(dir.filePath(QStringLiteral("a.json"))), QByteArray("two"));

    // A pending deletion replaced by a write writes.
    writer.schedule(QStringLiteral("a.json"), std::nullopt);
    writer.schedule(QStringLiteral("a.json"), QByteArray("three"));
    writer.flush();
    QCOMPARE(readFile(dir.filePath(QStringLiteral("a.json"))), QByteArray("three"));
}

void TestFileWriter::createsSubfolders()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileWriter writer(dir.path());

    writer.schedule(QStringLiteral("tasks/deep/a.json"), QByteArray("one"));
    writer.flush();
    QCOMPARE(readFile(dir.filePath(QStringLiteral("tasks/deep/a.json"))), QByteArray("one"));
}

void TestFileWriter::deletes()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("a.json"));
    QVERIFY(writeFile(path, "someone else's"));
    FileWriter writer(dir.path());
    QSignalSpy written(&writer, &FileWriter::written);
    QSignalSpy failed(&writer, &FileWriter::writeFailed);

    writer.schedule(QStringLiteral("a.json"), std::nullopt);
    writer.flush();
    QVERIFY(!QFile::exists(path));
    QCOMPARE(written.count(), 1);

    // Deleting a file that is already gone succeeds.
    writer.schedule(QStringLiteral("a.json"), std::nullopt);
    writer.flush();
    QCOMPARE(written.count(), 2);
    QCOMPARE(failed.count(), 0);
    QVERIFY(!writer.pending());
}

void TestFileWriter::ownWrites()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileWriter writer(dir.path());
    const QString a = QStringLiteral("a.json");

    QVERIFY(!writer.isOwnWrite(a, QByteArray()));
    QVERIFY(!writer.isOwnWrite(a, QByteArray("one")));

    // A pending write is not yet an own write.
    writer.schedule(a, QByteArray("one"));
    QVERIFY(!writer.isOwnWrite(a, QByteArray("one")));
    writer.flush();
    QVERIFY(writer.isOwnWrite(a, QByteArray("one")));
    QVERIFY(!writer.isOwnWrite(a, QByteArray("two")));
    QVERIFY(!writer.isOwnWrite(QStringLiteral("b.json"), QByteArray("one")));

    writer.schedule(a, QByteArray("two"));
    writer.flush();
    QVERIFY(writer.isOwnWrite(a, QByteArray("two")));
    QVERIFY(!writer.isOwnWrite(a, QByteArray("one")));

    // A deleted file matches an empty content.
    writer.schedule(a, std::nullopt);
    writer.flush();
    QVERIFY(writer.isOwnWrite(a, QByteArray()));
    QVERIFY(!writer.isOwnWrite(a, QByteArray("two")));
}

void TestFileWriter::failedWriteIsRetried()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    // The parent of the path is a file, so the write fails.
    const QString blocker = dir.filePath(QStringLiteral("tasks"));
    QVERIFY(writeFile(blocker, "not a folder"));
    FileWriter writer(dir.path());
    QSignalSpy written(&writer, &FileWriter::written);
    QSignalSpy failed(&writer, &FileWriter::writeFailed);
    const QString relPath = QStringLiteral("tasks/a.json");

    writer.schedule(relPath, QByteArray("one"));
    writer.schedule(QStringLiteral("b.json"), QByteArray("other"));
    writer.flush();
    QCOMPARE(failed.count(), 1);
    QCOMPARE(failed.at(0).at(0).toString(), relPath);
    QVERIFY(!failed.at(0).at(1).toString().isEmpty());
    // Other writes are not held up by the failure.
    QCOMPARE(written.count(), 1);
    QCOMPARE(written.at(0).at(0).toString(), QStringLiteral("b.json"));
    QVERIFY(writer.pending());
    QVERIFY(!writer.isOwnWrite(relPath, QByteArray("one")));

    // Failing again is not reported again.
    writer.flush();
    QCOMPARE(failed.count(), 1);
    QVERIFY(writer.pending());

    // Once the cause is fixed, the next flush writes it.
    QVERIFY(QFile::remove(blocker));
    writer.flush();
    QCOMPARE(written.count(), 2);
    QCOMPARE(written.at(1).at(0).toString(), relPath);
    QVERIFY(!writer.pending());
    QCOMPARE(readFile(dir.filePath(relPath)), QByteArray("one"));
    QVERIFY(writer.isOwnWrite(relPath, QByteArray("one")));

    // After a success, a new failure is reported again.
    QVERIFY(QDir(dir.path()).rename(QStringLiteral("tasks"), QStringLiteral("old-tasks")));
    QVERIFY(writeFile(blocker, "not a folder"));
    writer.schedule(relPath, QByteArray("two"));
    writer.flush();
    QCOMPARE(failed.count(), 2);
}

QTEST_GUILESS_MAIN(TestFileWriter)
#include "tst_file_writer.moc"
