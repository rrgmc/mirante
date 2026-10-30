// Tests ActionRow: clicks, visibility, busy, the error line, short labels in a
// narrow row, the menu action, and the "Web" link.
#include <mirante/widgets/ActionRow.h>

#include <QDesktopServices>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QTest>

using namespace mirante::widgets;

namespace {

QPushButton *button(const ActionRow &row, const QString &id)
{
    return row.findChild<QPushButton *>(id);
}

// The error line: the only label directly in the row.
QLabel *errorLine(const ActionRow &row)
{
    return row.findChild<QLabel *>(QString(), Qt::FindDirectChildrenOnly);
}

} // namespace

class TestActionRow : public QObject
{
    Q_OBJECT

public slots:
    // Takes the place of the browser, for the tests of "Web".
    void openUrl(const QUrl &url) { m_opened.append(url); }

private slots:
    void init()
    {
        m_opened.clear();
        QDesktopServices::setUrlHandler(QStringLiteral("https"), this, "openUrl");
    }

    void cleanup() { QDesktopServices::unsetUrlHandler(QStringLiteral("https")); }

    void clickRunsTheHandler()
    {
        ActionRow row;
        int runs = 0;
        row.addAction(QStringLiteral("approve"), QStringLiteral("Approve"), QStringLiteral("OK"),
                      [&runs] { ++runs; });
        button(row, QStringLiteral("approve"))->click();
        QCOMPARE(runs, 1);
    }

    void visibilityWorksBeforeTheFirstShow()
    {
        ActionRow row;
        row.addAction(QStringLiteral("approve"), QStringLiteral("Approve"), QString(), {});
        QVERIFY(row.isActionVisible(QStringLiteral("approve")));
        row.setActionVisible(QStringLiteral("approve"), false);
        QVERIFY(!row.isActionVisible(QStringLiteral("approve")));
        row.setActionVisible(QStringLiteral("approve"), true);
        QVERIFY(row.isActionVisible(QStringLiteral("approve")));
        QVERIFY(!row.isActionVisible(QStringLiteral("unknown")));
        row.setActionVisible(QStringLiteral("unknown"), true);
    }

    void busyDisablesTheButton()
    {
        ActionRow row;
        int runs = 0;
        row.addAction(QStringLiteral("approve"), QStringLiteral("Approve"), QString(),
                      [&runs] { ++runs; });
        row.addAction(QStringLiteral("comment"), QStringLiteral("Comment"), QString(), {});

        row.setBusy(QStringLiteral("approve"), true);
        QVERIFY(!button(row, QStringLiteral("approve"))->isEnabled());
        QVERIFY(button(row, QStringLiteral("comment"))->isEnabled());
        button(row, QStringLiteral("approve"))->click();
        QCOMPARE(runs, 0);

        row.setBusy(QStringLiteral("approve"), false);
        QVERIFY(button(row, QStringLiteral("approve"))->isEnabled());
    }

    void errorLineShowsAndClears()
    {
        ActionRow row;
        row.resize(300, 100);
        row.show();
        QVERIFY(QTest::qWaitForWindowExposed(&row));
        QVERIFY(errorLine(row));
        QVERIFY(!errorLine(row)->isVisible());
        QCOMPARE(errorLine(row)->textFormat(), Qt::PlainText);

        row.showError(QStringLiteral("<b>The token cannot do this.</b>"));
        QCOMPARE(row.error(), QStringLiteral("<b>The token cannot do this.</b>"));
        QVERIFY(errorLine(row)->isVisible());

        row.clearError();
        QVERIFY(row.error().isEmpty());
        QVERIFY(!errorLine(row)->isVisible());
    }

    void aNewActionClearsTheLastError()
    {
        ActionRow row;
        QString seen = QStringLiteral("unset");
        row.addAction(QStringLiteral("approve"), QStringLiteral("Approve"), QString(),
                      [&] { seen = row.error(); });
        row.showError(QStringLiteral("Failed"));
        button(row, QStringLiteral("approve"))->click();
        QVERIFY(seen.isEmpty());
    }

    void shortLabelsInANarrowRow()
    {
        ActionRow row;
        row.addAction(QStringLiteral("retry"), QStringLiteral("Retry the pipeline"),
                      QStringLiteral("Retry"), {});
        row.addAction(QStringLiteral("page"), QStringLiteral("Open the page"),
                      QStringLiteral("Page"), {});
        row.addAction(QStringLiteral("same"), QStringLiteral("Same"), QString(), {});
        QPushButton *retry = button(row, QStringLiteral("retry"));
        QPushButton *page = button(row, QStringLiteral("page"));

        row.resize(800, 100);
        row.show();
        QVERIFY(QTest::qWaitForWindowExposed(&row));
        QTRY_COMPARE(retry->text(), QStringLiteral("Retry the pipeline"));
        QCOMPARE(page->text(), QStringLiteral("Open the page"));

        row.resize(120, 100);
        QTRY_COMPARE(retry->text(), QStringLiteral("Retry"));
        QCOMPARE(page->text(), QStringLiteral("Page"));
        QCOMPARE(button(row, QStringLiteral("same"))->text(), QStringLiteral("Same"));
        // The row can really get that narrow, and wraps its buttons.
        QCOMPARE(row.width(), 120);
        QVERIFY(page->y() > retry->y() || page->x() > retry->x());
        QVERIFY(button(row, QStringLiteral("same"))->y() > retry->y());

        row.resize(800, 100);
        QTRY_COMPARE(retry->text(), QStringLiteral("Retry the pipeline"));
        QCOMPARE(button(row, QStringLiteral("same"))->y(), retry->y());
    }

    void aSingleLongActionAlsoGetsItsShortLabel()
    {
        ActionRow row;
        row.addAction(QStringLiteral("retry"), QStringLiteral("Retry the whole pipeline"),
                      QStringLiteral("Retry"), {});
        row.resize(40, 100);
        row.show();
        QVERIFY(QTest::qWaitForWindowExposed(&row));
        QTRY_COMPARE(button(row, QStringLiteral("retry"))->text(), QStringLiteral("Retry"));
    }

    void menuActionFillsItsMenuEachTime()
    {
        ActionRow row;
        int fills = 0;
        row.addMenuAction(QStringLiteral("status"), QStringLiteral("Change status"),
                          QStringLiteral("Status"), [&fills](QMenu &menu) {
                              ++fills;
                              menu.addAction(QStringLiteral("Entry %1").arg(fills));
                          });
        QMenu *menu = button(row, QStringLiteral("status"))->menu();
        QVERIFY(menu);

        emit menu->aboutToShow();
        QCOMPARE(menu->actions().size(), 1);
        emit menu->aboutToShow();
        QCOMPARE(fills, 2);
        QCOMPARE(menu->actions().size(), 1);
        QCOMPARE(menu->actions().first()->text(), QStringLiteral("Entry 2"));
    }

    void webIsHiddenWithoutAnAddress()
    {
        ActionRow row;
        QPushButton *web = button(row, QStringLiteral("web"));
        QVERIFY(web);
        QVERIFY(web->isHidden());
        QVERIFY(row.webLink().isEmpty());

        const QUrl url(QStringLiteral("https://gitlab.example.com/a/b/-/issues/1"));
        row.setWebLink(url);
        QCOMPARE(row.webLink(), url);
        QVERIFY(!web->isHidden());

        row.setWebLink(QUrl());
        QVERIFY(web->isHidden());
    }

    void webOpensTheAddress()
    {
        ActionRow row;
        const QUrl url(QStringLiteral("https://gitlab.example.com/a/b/-/issues/1"));
        row.setWebLink(url);
        button(row, QStringLiteral("web"))->click();
        QCOMPARE(m_opened, QList<QUrl>{url});
    }

    void webIgnoresOtherSchemes()
    {
        ActionRow row;
        row.setWebLink(QUrl(QStringLiteral("file:///C:/Windows/notepad.exe")));
        QVERIFY(button(row, QStringLiteral("web"))->isHidden());
    }

    void webStaysAtTheEndOfTheRow()
    {
        ActionRow row;
        row.setWebLink(QUrl(QStringLiteral("https://example.com")));
        row.addAction(QStringLiteral("approve"), QStringLiteral("Approve"), QString(), {});
        row.resize(800, 100);
        row.show();
        QVERIFY(QTest::qWaitForWindowExposed(&row));
        QTRY_VERIFY(button(row, QStringLiteral("web"))->x()
                    > button(row, QStringLiteral("approve"))->x());
    }

private:
    QList<QUrl> m_opened;
};

QTEST_MAIN(TestActionRow)
#include "tst_action_row.moc"
