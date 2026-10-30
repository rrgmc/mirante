// Tests statusColours() and StatusPill: a colour pair for each role in both
// modes with enough contrast, the choice by the palette's window lightness, and
// a pill painted in its colours as plain text.
#include <mirante/widgets/StatusPill.h>

#include <QTest>
#include <QtMath>

using namespace mirante::widgets;

namespace {

QPalette paletteWithWindow(const QColor &window)
{
    QPalette palette;
    palette.setColor(QPalette::Window, window);
    return palette;
}

QList<QPalette> bothModes()
{
    return {paletteWithWindow(QColor(0xf5, 0xf5, 0xf5)), paletteWithWindow(QColor(0x20, 0x20, 0x20))};
}

const QList<StatusRole> allRoles = {StatusRole::Good, StatusRole::Bad, StatusRole::Waiting,
                                    StatusRole::Neutral};

// The relative luminance of WCAG 2.
double luminance(const QColor &colour)
{
    const auto channel = [](double value) {
        return value <= 0.03928 ? value / 12.92 : qPow((value + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * channel(colour.redF()) + 0.7152 * channel(colour.greenF())
            + 0.0722 * channel(colour.blueF());
}

double contrast(const QColor &a, const QColor &b)
{
    const double first = luminance(a);
    const double second = luminance(b);
    return (qMax(first, second) + 0.05) / (qMin(first, second) + 0.05);
}

} // namespace

class TestStatusPill : public QObject
{
    Q_OBJECT

private slots:
    void eachRoleHasEnoughContrastInBothModes()
    {
        const QList<QPalette> palettes = bothModes();
        for (const QPalette &palette : palettes) {
            for (StatusRole role : allRoles) {
                const StatusColours colours = statusColours(role, palette);
                QVERIFY(contrast(colours.fill, colours.text) >= 4.5);
            }
        }
    }

    // A transparent window colour says nothing, so the text colour decides.
    void transparentWindowFollowsTheTextColour()
    {
        const QColor transparent(0, 0, 0, 0);
        QPalette light;
        light.setColor(QPalette::Window, transparent);
        light.setColor(QPalette::WindowText, Qt::black);
        QPalette dark = light;
        dark.setColor(QPalette::WindowText, Qt::white);
        QCOMPARE(statusColours(StatusRole::Good, light).fill,
                 statusColours(StatusRole::Good, paletteWithWindow(Qt::white)).fill);
        QCOMPARE(statusColours(StatusRole::Good, dark).fill,
                 statusColours(StatusRole::Good, paletteWithWindow(Qt::black)).fill);
    }

    void modesAndRolesDiffer()
    {
        const QList<QPalette> palettes = bothModes();
        for (StatusRole role : allRoles) {
            const StatusColours light = statusColours(role, palettes[0]);
            const StatusColours dark = statusColours(role, palettes[1]);
            QVERIFY(light.fill != dark.fill);
            QVERIFY(light.fill.lightness() > 128);
            QVERIFY(dark.fill.lightness() < 128);
        }
        for (const QPalette &palette : palettes) {
            QVERIFY(statusColours(StatusRole::Good, palette).fill
                    != statusColours(StatusRole::Bad, palette).fill);
            QVERIFY(statusColours(StatusRole::Bad, palette).fill
                    != statusColours(StatusRole::Waiting, palette).fill);
            QVERIFY(statusColours(StatusRole::Waiting, palette).fill
                    != statusColours(StatusRole::Neutral, palette).fill);
        }
    }

    void pillPaintsItsFill()
    {
        const QList<QPalette> palettes = bothModes();
        for (const QPalette &palette : palettes) {
            StatusPill pill(QStringLiteral("Failed"), StatusRole::Bad);
            pill.setPalette(palette);
            const QImage image = pill.grab().toImage();
            // The line above the text, away from the rounded ends.
            QCOMPARE(image.pixelColor(image.width() / 2, 1).rgb(),
                     statusColours(StatusRole::Bad, palette).fill.rgb());
        }
    }

    void setStatusChangesTextAndRole()
    {
        StatusPill pill(QStringLiteral("Running"), StatusRole::Waiting);
        QCOMPARE(pill.role(), StatusRole::Waiting);
        pill.setStatus(QStringLiteral("Passed"), StatusRole::Good);
        QCOMPARE(pill.text(), QStringLiteral("Passed"));
        QCOMPARE(pill.role(), StatusRole::Good);
    }

    void textIsPlain()
    {
        StatusPill pill(QStringLiteral("<b>x</b>"), StatusRole::Neutral);
        QCOMPARE(pill.textFormat(), Qt::PlainText);
    }
};

QTEST_MAIN(TestStatusPill)
#include "tst_status_pill.moc"
