#include "core/LogBuffer.h"
#include "ui/LogWindow.h"

#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QTest>

using namespace mirante;

namespace {

LogEntry entry(QtMsgType type, const QString &category, const QString &message)
{
    return LogEntry{QDateTime::currentDateTime(), type, category, message};
}

} // namespace

class TestLogWindow : public QObject
{
    Q_OBJECT

private slots:
    void showsKeptAndNewMessages()
    {
        LogBuffer buffer;
        buffer.add(entry(QtInfoMsg, QStringLiteral("default"), QStringLiteral("before the window")));
        LogWindow window(buffer);
        QVERIFY(window.shownText().contains(QStringLiteral("info: before the window")));
        buffer.add(entry(QtWarningMsg, QStringLiteral("js"), QStringLiteral("a page said this")));
        QVERIFY(window.shownText().contains(QStringLiteral("warning js: a page said this")));
    }

    void filters()
    {
        LogBuffer buffer;
        buffer.add(entry(QtDebugMsg, QString(), QStringLiteral("quiet")));
        buffer.add(entry(QtWarningMsg, QStringLiteral("js"), QStringLiteral("claude.ai said no")));
        buffer.add(entry(QtInfoMsg, QString(), QStringLiteral("gitlab loaded")));
        LogWindow window(buffer);

        auto *level = window.findChild<QComboBox *>(QStringLiteral("logLevel"));
        QVERIFY(level);
        level->setCurrentIndex(2);      // warnings and above
        QVERIFY(!window.shownText().contains(QStringLiteral("quiet")));
        QVERIFY(!window.shownText().contains(QStringLiteral("gitlab")));
        QVERIFY(window.shownText().contains(QStringLiteral("claude.ai")));

        level->setCurrentIndex(0);
        auto *filter = window.findChild<QLineEdit *>(QStringLiteral("logFilter"));
        QVERIFY(filter);
        filter->setText(QStringLiteral("GITLAB"));
        QVERIFY(window.shownText().contains(QStringLiteral("gitlab loaded")));
        QVERIFY(!window.shownText().contains(QStringLiteral("claude.ai")));
    }

    void clearEmptiesBufferAndView()
    {
        LogBuffer buffer;
        buffer.add(entry(QtInfoMsg, QString(), QStringLiteral("something")));
        LogWindow window(buffer);
        auto *clear = window.findChild<QPushButton *>(QStringLiteral("logClear"));
        QVERIFY(clear);
        clear->click();
        QVERIFY(buffer.entries().empty());
        QVERIFY(window.shownText().isEmpty());
    }
};

QTEST_MAIN(TestLogWindow)
#include "tst_log_window.moc"
