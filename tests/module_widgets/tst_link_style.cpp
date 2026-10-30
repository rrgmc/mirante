// Tests that a button styled as a link asks for the palette's link colour and no
// frame, and that ActionRow's links and "Web" use the style. Whether it looks
// right is a check by hand: the offscreen platform has no fonts to draw text with.
#include <mirante/widgets/ActionRow.h>
#include <mirante/widgets/LinkStyle.h>

#include <QPushButton>
#include <QTest>
#include <QToolButton>

using namespace mirante::widgets;

namespace {

bool looksLikeALink(const QAbstractButton *button)
{
    const QString sheet = button->styleSheet();
    return sheet.contains(QStringLiteral("color: palette(link)")) && sheet.contains(QStringLiteral("border: none"))
        && button->cursor().shape() == Qt::PointingHandCursor;
}

} // namespace

class TestLinkStyle : public QObject
{
    Q_OBJECT

private slots:
    void bothKindsOfButtonLookLikeLinks()
    {
        QPushButton push;
        styleAsLink(&push);
        QVERIFY(looksLikeALink(&push));
        QCOMPARE(push.focusPolicy(), Qt::TabFocus);

        QToolButton tool;
        styleAsLink(&tool);
        QVERIFY(looksLikeALink(&tool));
    }

    void actionRowUsesTheStyleForLinksOnly()
    {
        ActionRow row;
        row.addAction(QStringLiteral("command"), QStringLiteral("Command"), {}, {});
        row.addLinkAction(QStringLiteral("link"), QStringLiteral("Link"), {}, {});
        const auto *command = row.findChild<QPushButton *>(QStringLiteral("command"));
        const auto *link = row.findChild<QPushButton *>(QStringLiteral("link"));
        const auto *web = row.findChild<QPushButton *>(QStringLiteral("web"));
        QVERIFY(command && link && web);
        QVERIFY(!looksLikeALink(command));
        QVERIFY(looksLikeALink(link));
        QVERIFY(looksLikeALink(web));
    }
};

QTEST_MAIN(TestLinkStyle)
#include "tst_link_style.moc"
