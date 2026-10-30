// Tests GlobalHotkey: an empty sequence removes the registration, and a hotkey
// either registers or says why it could not.
#include "platform/GlobalHotkey.h"

#include <QTest>

using namespace mirante::platform;

class TestGlobalHotkey : public QObject
{
    Q_OBJECT

private slots:
    void emptySequenceSucceeds()
    {
        GlobalHotkey hotkey;
        QString error;
        QVERIFY(hotkey.setShortcut(QKeySequence(), &error));
        QVERIFY(error.isEmpty());
    }

    // A key combination that nothing else on the machine should hold. Where the
    // platform has no global hotkeys, as with Qt's offscreen platform on Linux,
    // the failure names the hotkey.
    void registerOrReport()
    {
        GlobalHotkey hotkey;
        const QKeySequence sequence(Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_F19);
        QString error;
        const bool ok = hotkey.setShortcut(sequence, &error);
        if (GlobalHotkey::isSupported()) {
            QVERIFY2(ok, qPrintable(error));
            QVERIFY(error.isEmpty());
            QVERIFY(hotkey.setShortcut(QKeySequence(), &error));
        } else {
            QVERIFY(!ok);
            QVERIFY(error.contains(sequence.toString(QKeySequence::NativeText)));
        }
    }
};

QTEST_MAIN(TestGlobalHotkey)
#include "tst_global_hotkey.moc"
