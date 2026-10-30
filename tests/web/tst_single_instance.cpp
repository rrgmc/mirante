// Tests SingleInstance in temporary folders: the lock, the handover of a
// later start to the holder, independent folders, and a lock left by a
// process that no longer runs. The later start runs on its own thread,
// because the holder answers from the event loop.
#include "web/SingleInstance.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QLockFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <atomic>
#include <thread>

using namespace mirante;

namespace {

// Runs handOver() on another thread, so the holder's event loop can answer.
class HandOver
{
public:
    HandOver(const SingleInstance &instance, const QString &dataFolder, int timeoutMs)
        : m_thread([this, &instance, dataFolder, timeoutMs] {
            m_result = instance.handOver(dataFolder, timeoutMs) ? 1 : 0;
        })
    {
    }
    ~HandOver() { m_thread.join(); }

    bool done() const { return m_result >= 0; }
    bool succeeded() const { return m_result == 1; }

private:
    std::atomic<int> m_result{-1};
    std::thread m_thread;
};

} // namespace

class TestSingleInstance : public QObject
{
    Q_OBJECT

private slots:
    void firstTakesTheLock()
    {
        QTemporaryDir dir;
        const QString folder = dir.filePath(QStringLiteral("profiles"));
        SingleInstance first(folder);
        QVERIFY(!first.isLocked());
        QVERIFY(first.tryLock());
        QVERIFY(first.isLocked());
        // The folder is made, and holds the lock file.
        QVERIFY(QFile::exists(QDir(folder).filePath(QStringLiteral(".lock"))));
    }

    void secondHandsOverToTheFirst()
    {
        QTemporaryDir dir;
        SingleInstance first(dir.path());
        QVERIFY(first.tryLock());
        QSignalSpy requested(&first, &SingleInstance::activationRequested);

        SingleInstance second(dir.path());
        QVERIFY(!second.tryLock());
        QVERIFY(second.heldElsewhere());
        QCOMPARE(second.holderPid(), QCoreApplication::applicationPid());

        const QString data = QDir(dir.path()).filePath(QStringLiteral("my workspace"));
        {
            HandOver handOver(second, data, 5000);
            QTRY_VERIFY(handOver.done());
            QVERIFY(handOver.succeeded());
        }
        QTRY_COMPARE(requested.count(), 1);
        QCOMPARE(requested.at(0).at(0).toString(), data);
    }

    void handOverWithoutData()
    {
        QTemporaryDir dir;
        SingleInstance first(dir.path());
        QVERIFY(first.tryLock());
        QSignalSpy requested(&first, &SingleInstance::activationRequested);

        SingleInstance second(dir.path());
        QVERIFY(!second.tryLock());
        {
            HandOver handOver(second, QString(), 5000);
            QTRY_VERIFY(handOver.done());
            QVERIFY(handOver.succeeded());
        }
        QTRY_COMPARE(requested.count(), 1);
        QCOMPARE(requested.at(0).at(0).toString(), QString());
    }

    void handOverFailsWhenNobodyAnswers()
    {
        // A holder that does not listen, as one that hangs at start.
        QTemporaryDir dir;
        QLockFile lock(QDir(dir.path()).filePath(QStringLiteral(".lock")));
        QVERIFY(lock.tryLock(0));

        SingleInstance second(dir.path());
        QVERIFY(!second.tryLock());
        QVERIFY(second.heldElsewhere());
        QVERIFY(!second.handOver(QString(), 300));
    }

    void otherFolderIsIndependent()
    {
        QTemporaryDir one;
        QTemporaryDir two;
        SingleInstance first(one.path());
        SingleInstance other(two.path());
        QVERIFY(first.tryLock());
        QVERIFY(other.tryLock());
        QVERIFY(SingleInstance::serverName(one.path()) != SingleInstance::serverName(two.path()));

        // A later start in the second folder reaches the second holder only.
        QSignalSpy firstRequested(&first, &SingleInstance::activationRequested);
        QSignalSpy otherRequested(&other, &SingleInstance::activationRequested);
        SingleInstance later(two.path());
        QVERIFY(!later.tryLock());
        {
            HandOver handOver(later, QStringLiteral("x"), 5000);
            QTRY_VERIFY(handOver.done());
            QVERIFY(handOver.succeeded());
        }
        QTRY_COMPARE(otherRequested.count(), 1);
        QCOMPARE(firstRequested.count(), 0);
    }

    void serverNameIgnoresHowThePathIsWritten()
    {
        QTemporaryDir dir;
        const QString path = dir.path();
        QCOMPARE(SingleInstance::serverName(path + QStringLiteral("/")), SingleInstance::serverName(path));
        QCOMPARE(SingleInstance::serverName(path + QStringLiteral("/sub/..")), SingleInstance::serverName(path));
        QVERIFY(SingleInstance::serverName(path).startsWith(QStringLiteral("mirante-")));
    }

    void staleLockIsTakenOver()
    {
        // A lock file as QLockFile writes it, by a process id that does not
        // run: what a crash leaves behind.
        QTemporaryDir dir;
        QFile file(QDir(dir.path()).filePath(QStringLiteral(".lock")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("2147483644\nmirante\n\n");
        file.close();

        SingleInstance instance(dir.path());
        QVERIFY(instance.tryLock());
        QVERIFY(instance.isLocked());
    }

    void lockGoesWithTheHolder()
    {
        QTemporaryDir dir;
        {
            SingleInstance first(dir.path());
            QVERIFY(first.tryLock());
        }
        SingleInstance next(dir.path());
        QVERIFY(next.tryLock());
    }
};

QTEST_GUILESS_MAIN(TestSingleInstance)
#include "tst_single_instance.moc"
