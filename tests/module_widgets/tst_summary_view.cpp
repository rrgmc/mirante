// Tests SummaryView: the order of the top and the sections, replacing, hiding,
// the scroll position across updates, the message, and that a narrow view does
// not need a horizontal scroll bar.
#include <mirante/widgets/ActionRow.h>
#include <mirante/widgets/SummaryView.h>

#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QScrollBar>
#include <QTest>
#include <QVBoxLayout>

using namespace mirante::widgets;

namespace {

QWidget *tallWidget(int height = 300)
{
    auto *widget = new QWidget;
    widget->setFixedHeight(height);
    return widget;
}

QLabel *labelWithText(const SummaryView &view, const QString &text)
{
    const auto labels = view.findChildren<QLabel *>();
    for (QLabel *label : labels) {
        if (label->text() == text)
            return label;
    }
    return nullptr;
}

int yOf(const SummaryView &view, QWidget *widget)
{
    return widget->mapTo(view.widget(), QPoint()).y();
}

} // namespace

class TestSummaryView : public QObject
{
    Q_OBJECT

private slots:
    void topComesBeforeTheSections()
    {
        SummaryView view;
        view.setTitle(QStringLiteral("The title"));
        auto *state = new QLabel(QStringLiteral("The state"));
        view.setStateRow(state);
        view.actions()->addAction(QStringLiteral("go"), QStringLiteral("Go"), QString(), {});
        view.setSection(QStringLiteral("a"), QStringLiteral("Section A"), tallWidget(50));
        view.resize(400, 400);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));

        QLabel *title = labelWithText(view, QStringLiteral("The title"));
        QVERIFY(title);
        QCOMPARE(title->textFormat(), Qt::PlainText);
        QTRY_VERIFY(yOf(view, title) < yOf(view, state));
        QVERIFY(yOf(view, state) < yOf(view, view.actions()));
        QVERIFY(yOf(view, view.actions()) < yOf(view, labelWithText(view, QStringLiteral("Section A"))));
    }

    void sectionsKeepTheOrderOfFirstUse()
    {
        SummaryView view;
        auto *first = tallWidget(50);
        auto *second = tallWidget(50);
        auto *third = tallWidget(50);
        view.setSection(QStringLiteral("b"), QStringLiteral("B"), first);
        view.setSection(QStringLiteral("a"), QStringLiteral("A"), second);
        view.setSection(QStringLiteral("c"), QStringLiteral("C"), third);
        view.resize(400, 400);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        QTRY_VERIFY(yOf(view, first) < yOf(view, second));
        QVERIFY(yOf(view, second) < yOf(view, third));

        // Replacing the first keeps its place.
        auto *replacement = tallWidget(50);
        view.setSection(QStringLiteral("b"), QStringLiteral("B"), replacement);
        QTRY_VERIFY(yOf(view, replacement) < yOf(view, second));
    }

    void replacingDeletesTheOldContentAndChangesTheTitle()
    {
        SummaryView view;
        QPointer<QWidget> old = tallWidget(50);
        view.setSection(QStringLiteral("a"), QStringLiteral("Old title"), old);
        auto *replacement = tallWidget(50);
        view.setSection(QStringLiteral("a"), QStringLiteral("New title"), replacement);

        QVERIFY(!labelWithText(view, QStringLiteral("Old title")));
        QVERIFY(labelWithText(view, QStringLiteral("New title")));
        QVERIFY(replacement->parentWidget());
        QVERIFY(old);
        QVERIFY(!old->parent());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!old);
    }

    void aSectionCanReplaceItselfFromAButtonInsideIt()
    {
        SummaryView view;
        auto *button = new QPushButton(QStringLiteral("Retry"));
        view.setSection(QStringLiteral("a"), QStringLiteral("A"), button);
        connect(button, &QPushButton::clicked, &view, [&view] {
            view.setSection(QStringLiteral("a"), QStringLiteral("A"), new QLabel(QStringLiteral("Done")));
        });
        view.resize(400, 400);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        button->click();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(labelWithText(view, QStringLiteral("Done")));
    }

    void sectionsHideAndComeBack()
    {
        SummaryView view;
        QWidget *content = tallWidget(50);
        QVERIFY(!view.isSectionVisible(QStringLiteral("a")));
        view.setSection(QStringLiteral("a"), QStringLiteral("A"), content);
        QVERIFY(view.isSectionVisible(QStringLiteral("a")));

        view.resize(400, 400);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        QVERIFY(content->isVisible());

        view.setSectionVisible(QStringLiteral("a"), false);
        QVERIFY(!view.isSectionVisible(QStringLiteral("a")));
        QVERIFY(!content->isVisible());
        QVERIFY(!labelWithText(view, QStringLiteral("A"))->isVisible());

        // New content does not bring the section back.
        QWidget *replacement = tallWidget(50);
        view.setSection(QStringLiteral("a"), QStringLiteral("A"), replacement);
        QVERIFY(!view.isSectionVisible(QStringLiteral("a")));
        QVERIFY(!replacement->isVisible());

        view.setSectionVisible(QStringLiteral("a"), true);
        QVERIFY(view.isSectionVisible(QStringLiteral("a")));
        QVERIFY(replacement->isVisible());
    }

    void aSectionHiddenBeforeItsFirstUseStaysHidden()
    {
        SummaryView view;
        view.setSectionVisible(QStringLiteral("a"), false);
        view.setSection(QStringLiteral("a"), QStringLiteral("A"), tallWidget(50));
        QVERIFY(!view.isSectionVisible(QStringLiteral("a")));
    }

    void scrollPositionIsKeptAcrossUpdates()
    {
        SummaryView view;
        view.setSection(QStringLiteral("a"), QStringLiteral("A"), tallWidget());
        view.setSection(QStringLiteral("b"), QStringLiteral("B"), tallWidget());
        view.setSection(QStringLiteral("c"), QStringLiteral("C"), tallWidget());
        view.resize(300, 200);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        QTRY_VERIFY(view.verticalScrollBar()->maximum() > 400);

        view.verticalScrollBar()->setValue(400);
        for (const QString &key : {QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")}) {
            view.setSection(key, key.toUpper(), tallWidget());
            QCoreApplication::processEvents();
            QCoreApplication::processEvents();
            QCOMPARE(view.verticalScrollBar()->value(), 400);
        }
        view.setStateRow(new QLabel(QStringLiteral("State")));
        QCoreApplication::processEvents();
        QCOMPARE(view.verticalScrollBar()->value(), 400);
    }

    void scrollPositionIsKeptWhenAFocusedButtonGoes()
    {
        SummaryView view;
        view.actions()->addAction(QStringLiteral("go"), QStringLiteral("Go"), QString(), {});
        auto *button = new QPushButton(QStringLiteral("Retry"));
        button->setMinimumHeight(300);
        view.setSection(QStringLiteral("a"), QStringLiteral("A"), tallWidget());
        view.setSection(QStringLiteral("b"), QStringLiteral("B"), button);
        view.setSection(QStringLiteral("c"), QStringLiteral("C"), tallWidget());
        view.resize(300, 200);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        view.activateWindow();
        QTRY_VERIFY(view.verticalScrollBar()->maximum() > 400);

        button->setFocus();
        view.verticalScrollBar()->setValue(400);
        auto *replacement = new QPushButton(QStringLiteral("Retry"));
        replacement->setMinimumHeight(300);
        view.setSection(QStringLiteral("b"), QStringLiteral("B"), replacement);
        QCOMPARE(view.verticalScrollBar()->value(), 400);
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();
        QCOMPARE(view.verticalScrollBar()->value(), 400);
    }

    void messageShowsInsteadOfEverythingElse()
    {
        SummaryView view;
        view.setTitle(QStringLiteral("The title"));
        view.resize(400, 400);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));

        view.setMessage(QStringLiteral("Loading…"));
        QLabel *message = labelWithText(view, QStringLiteral("Loading…"));
        QVERIFY(message);
        QVERIFY(message->isVisible());
        QCOMPARE(message->textFormat(), Qt::PlainText);
        QVERIFY(!labelWithText(view, QStringLiteral("The title"))->isVisible());
        QVERIFY(!view.actions()->isVisible());

        view.setMessage(QString());
        QVERIFY(!message->isVisible());
        QVERIFY(labelWithText(view, QStringLiteral("The title"))->isVisible());
        QVERIFY(view.actions()->isVisible());
    }

    void aNarrowViewWrapsInsteadOfScrollingSideways()
    {
        SummaryView view;
        view.setTitle(QStringLiteral("A title with several words that wrap in a narrow cell"));
        for (int i = 0; i < 6; ++i) {
            view.actions()->addAction(QStringLiteral("action%1").arg(i),
                                      QStringLiteral("A long action %1").arg(i),
                                      QStringLiteral("Act %1").arg(i), {});
        }
        view.setSection(QStringLiteral("a"), QStringLiteral("A"), tallWidget(20));
        view.resize(120, 500);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        auto *last = view.findChild<QPushButton *>(QStringLiteral("action5"));
        QVERIFY(last);
        QTRY_COMPARE(last->text(), QStringLiteral("Act 5"));
        QTRY_COMPARE(view.horizontalScrollBar()->maximum(), 0);
        // The buttons take several rows, and the section starts below them.
        QTRY_VERIFY(last->y() > 0);
        QVERIFY(yOf(view, labelWithText(view, QStringLiteral("A"))) > yOf(view, last));
    }
};

QTEST_MAIN(TestSummaryView)
#include "tst_summary_view.moc"
