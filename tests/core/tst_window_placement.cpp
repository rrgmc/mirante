// Tests windowplacement on fake screens: a saved window that is still visible
// keeps its place; one on an unplugged monitor, one cut off by a smaller
// resolution and one with its title bar out of reach fall back to the primary
// screen, centred at the saved size.
#include "core/WindowPlacement.h"

#include <QTest>

using namespace mirante;
using namespace mirante::windowplacement;

namespace {

// Two screens side by side: a laptop on the left and a monitor on the right.
// Their available geometries leave room for a menu bar or a task bar.
const QRect kLaptop(0, 25, 1440, 875);
const QRect kMonitor(1440, 0, 2560, 1400);

} // namespace

class TestWindowPlacement : public QObject
{
    Q_OBJECT

private slots:
    void visibleOnOneScreen()
    {
        QVERIFY(isVisible(QRect(100, 100, 1000, 600), {kLaptop}));
        QVERIFY(isVisible(QRect(2000, 200, 1200, 800), {kLaptop, kMonitor}));
    }

    // A window across two screens is visible when its title bar is wholly on them.
    void visibleAcrossScreens()
    {
        QVERIFY(isVisible(QRect(1000, 100, 1000, 600), {kLaptop, kMonitor}));
    }

    // The monitor was unplugged: the window that was on it is not visible.
    void unpluggedMonitor()
    {
        const QRect saved(2000, 200, 1200, 800);
        QVERIFY(!isVisible(saved, {kLaptop}));
        const QRect placed = place(saved, QSize(), {kLaptop}, kLaptop);
        QCOMPARE(placed.size(), saved.size());
        QVERIFY(kLaptop.contains(placed));
        QVERIFY(isVisible(placed, {kLaptop}));
    }

    // The title bar must be reachable to move the window.
    void titleBarOffScreen()
    {
        // Above the top of the screen, under the menu bar.
        QVERIFY(!isVisible(QRect(100, 30, 800, 600), {kLaptop}));
        // Partly past the left edge.
        QVERIFY(!isVisible(QRect(-200, 300, 800, 500), {kLaptop}));
        // In the gap of two screens that do not meet.
        const QRect left(0, 0, 1000, 800);
        const QRect right(1100, 0, 1000, 800);
        QVERIFY(!isVisible(QRect(800, 200, 600, 400), {left, right}));
    }

    // The title bar is on the screen, but most of the window hangs below it.
    void mostlyOffScreen()
    {
        QVERIFY(!isVisible(QRect(100, 700, 800, 600), {kLaptop}));
        QVERIFY(isVisible(QRect(100, 400, 800, 600), {kLaptop}));
    }

    // The resolution shrank: the window no longer fits, so it is centred on
    // the primary screen and made to fit it.
    void resolutionShrank()
    {
        const QRect saved(0, 57, 1920, 1100);
        const QRect small(0, 25, 1280, 775);
        QVERIFY(!isVisible(saved, {small}));
        const QRect placed = place(saved, QSize(), {small}, small);
        QCOMPARE(placed.width(), 1280);
        QCOMPARE(placed.height(), 775 - titleBarHeight);
        QCOMPARE(placed.top() - titleBarHeight, small.top());
        QVERIFY(isVisible(placed, {small}));
    }

    void keepsVisiblePlace()
    {
        const QRect saved(2000, 200, 1200, 800);
        QCOMPARE(place(saved, QSize(), {kLaptop, kMonitor}, kLaptop), saved);
    }

    // Without a saved place, as from earlier versions, the saved size is
    // centred on the primary screen.
    void noSavedPlace()
    {
        const QRect placed = place(QRect(), QSize(1000, 600), {kLaptop, kMonitor}, kLaptop);
        QCOMPARE(placed.size(), QSize(1000, 600));
        const QRect frame = placed.adjusted(0, -titleBarHeight, 0, 0);
        QVERIFY(qAbs(frame.center().x() - kLaptop.center().x()) <= 1);
        QVERIFY(qAbs(frame.center().y() - kLaptop.center().y()) <= 1);
    }

    // Mirrored screens overlap; the covered area is not counted twice.
    void overlappingScreens()
    {
        const QRect screen(0, 0, 1000, 800);
        QVERIFY(!isVisible(QRect(100, 500, 800, 700), {screen, screen}));
        QVERIFY(isVisible(QRect(100, 100, 600, 400), {screen, screen}));
    }

    void noScreens()
    {
        QVERIFY(!isVisible(QRect(0, 50, 800, 600), {}));
        QCOMPARE(place(QRect(0, 50, 800, 600), QSize(), {}, QRect()).size(), QSize(800, 600));
    }
};

QTEST_GUILESS_MAIN(TestWindowPlacement)
#include "tst_window_placement.moc"
