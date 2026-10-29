#include "core/LogBuffer.h"

#include <QLoggingCategory>
#include <QSignalSpy>
#include <QTest>
#include <QThread>

using namespace mirante;

namespace {

Q_LOGGING_CATEGORY(testCategory, "mirante.test")

// The handler that was installed when a test ran, to see that the buffer
// passes messages on.
QStringList passedOn;
void recordingHandler(QtMsgType, const QMessageLogContext &, const QString &message) { passedOn << message; }

} // namespace

class TestLogBuffer : public QObject
{
    Q_OBJECT

private slots:
    void init() { passedOn.clear(); }

    void keepsMessagesAndPassesThemOn()
    {
        const QtMessageHandler before = qInstallMessageHandler(recordingHandler);
        {
            LogBuffer buffer;
            buffer.install();
            QCOMPARE(LogBuffer::installedBuffer(), &buffer);
            QSignalSpy added(&buffer, &LogBuffer::entryAdded);
            qInfo("hello");
            qCWarning(testCategory) << "careful";
            const auto entries = buffer.entries();
            QCOMPARE(entries.size(), size_t(2));
            QCOMPARE(entries[0].type, QtInfoMsg);
            QCOMPARE(entries[0].message, QStringLiteral("hello"));
            QCOMPARE(entries[1].type, QtWarningMsg);
            QCOMPARE(entries[1].category, QStringLiteral("mirante.test"));
            QCOMPARE(added.count(), 2);
            QCOMPARE(passedOn, (QStringList{QStringLiteral("hello"), QStringLiteral("careful")}));
        }
        // The buffer put the earlier handler back when it went.
        QVERIFY(!LogBuffer::installedBuffer());
        qInfo("after");
        QCOMPARE(passedOn.last(), QStringLiteral("after"));
        qInstallMessageHandler(before);
    }

    void dropsTheOldest()
    {
        LogBuffer buffer(3);
        for (int i = 0; i < 5; ++i)
            buffer.add(LogEntry{QDateTime::currentDateTime(), QtDebugMsg, QString(), QString::number(i)});
        const auto entries = buffer.entries();
        QCOMPARE(entries.size(), size_t(3));
        QCOMPARE(entries.front().message, QStringLiteral("2"));
        QCOMPARE(entries.back().message, QStringLiteral("4"));
    }

    void clear()
    {
        LogBuffer buffer;
        buffer.add(LogEntry{QDateTime::currentDateTime(), QtDebugMsg, QString(), QStringLiteral("x")});
        QSignalSpy cleared(&buffer, &LogBuffer::cleared);
        buffer.clear();
        QVERIFY(buffer.entries().empty());
        QCOMPARE(cleared.count(), 1);
    }

    // A message from another thread is stored at once, and signalled on the
    // buffer's thread through the event loop.
    void messageFromAnotherThread()
    {
        const QtMessageHandler before = qInstallMessageHandler(recordingHandler);
        LogBuffer buffer;
        buffer.install();
        QSignalSpy added(&buffer, &LogBuffer::entryAdded);
        QThread *thread = QThread::create([] { qWarning("from a thread"); });
        thread->start();
        thread->wait();
        delete thread;
        QCOMPARE(buffer.entries().size(), size_t(1));
        QTRY_COMPARE(added.count(), 1);
        QCOMPARE(added.at(0).at(0).value<LogEntry>().message, QStringLiteral("from a thread"));
        qInstallMessageHandler(before);
    }
};

QTEST_GUILESS_MAIN(TestLogBuffer)
#include "tst_log_buffer.moc"
