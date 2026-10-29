// Tests platform::attachTarget: on macOS, a modal dialog of a visible window is
// attached to that window; a dialog of a dialog, a non-modal dialog and a
// dialog without a parent are not. Elsewhere no dialog is attached.
#include "platform/AttachedDialogs.h"

#include <QDialog>
#include <QTest>
#include <QWidget>

using namespace mirante;

class TestAttachedDialogs : public QObject
{
    Q_OBJECT

private slots:
    void attachesOnlyDialogsOfWindows()
    {
        QWidget window;
        window.resize(400, 300);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        QDialog dialog(&window);
        dialog.setWindowModality(Qt::ApplicationModal);
        QDialog nested(&dialog);
        nested.setWindowModality(Qt::ApplicationModal);
        QDialog modeless(&window);
        QDialog orphan;
        orphan.setWindowModality(Qt::ApplicationModal);

#if defined(Q_OS_MACOS)
        QCOMPARE(platform::attachTarget(&dialog), &window);
        // exec() marks the dialog with WA_ShowModal before its modality.
        QDialog execed(&window);
        execed.setAttribute(Qt::WA_ShowModal);
        QCOMPARE(platform::attachTarget(&execed), &window);
#else
        QCOMPARE(platform::attachTarget(&dialog), nullptr);
#endif
        QCOMPARE(platform::attachTarget(&nested), nullptr);
        QCOMPARE(platform::attachTarget(&modeless), nullptr);
        QCOMPARE(platform::attachTarget(&orphan), nullptr);
        QCOMPARE(platform::attachTarget(nullptr), nullptr);
    }
};

QTEST_MAIN(TestAttachedDialogs)
#include "tst_attached_dialogs.moc"
