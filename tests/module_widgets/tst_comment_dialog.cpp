// Tests askComment(): the text comes back, cancelling gives nothing, empty text
// cannot be sent, and the dialog survives its parent going.
#include <mirante/widgets/CommentDialog.h>

#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTest>
#include <QTimer>

#include <functional>

using namespace mirante::widgets;

namespace {

// Runs a step inside the dialog's event loop.
void whenOpen(std::function<void(QDialog &)> step)
{
    QTimer::singleShot(0, qApp, [step = std::move(step)] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        if (!dialog) {
            QFAIL("no dialog is open");
            return;
        }
        step(*dialog);
    });
}

QPushButton *sendButton(QDialog &dialog)
{
    return dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
}

} // namespace

class TestCommentDialog : public QObject
{
    Q_OBJECT

private slots:
    void returnsTheText()
    {
        whenOpen([](QDialog &dialog) {
            QCOMPARE(dialog.windowTitle(), QStringLiteral("Comment on !12"));
            dialog.findChild<QPlainTextEdit *>()->setPlainText(QStringLiteral("Looks good\nto me"));
            sendButton(dialog)->click();
        });
        const std::optional<QString> text = askComment(nullptr, QStringLiteral("Comment on !12"));
        QVERIFY(text);
        QCOMPARE(*text, QStringLiteral("Looks good\nto me"));
    }

    void cancellingGivesNothing()
    {
        whenOpen([](QDialog &dialog) {
            dialog.findChild<QPlainTextEdit *>()->setPlainText(QStringLiteral("Never mind"));
            dialog.reject();
        });
        QVERIFY(!askComment(nullptr, QStringLiteral("Comment")));
    }

    void emptyTextCannotBeSent()
    {
        bool checked = false;
        whenOpen([&checked](QDialog &dialog) {
            auto *text = dialog.findChild<QPlainTextEdit *>();
            QVERIFY(!sendButton(dialog)->isEnabled());
            text->setPlainText(QStringLiteral("  \n "));
            QVERIFY(!sendButton(dialog)->isEnabled());
            text->setPlainText(QStringLiteral("x"));
            QVERIFY(sendButton(dialog)->isEnabled());
            text->clear();
            QVERIFY(!sendButton(dialog)->isEnabled());
            checked = true;
            dialog.reject();
        });
        QVERIFY(!askComment(nullptr, QStringLiteral("Comment")));
        QVERIFY(checked);
    }

    void survivesItsParentGoing()
    {
        auto *parent = new QWidget;
        parent->show();
        QVERIFY(QTest::qWaitForWindowExposed(parent));
        whenOpen([parent](QDialog &) { delete parent; });
        QVERIFY(!askComment(parent, QStringLiteral("Comment")));
    }
};

QTEST_MAIN(TestCommentDialog)
#include "tst_comment_dialog.moc"
