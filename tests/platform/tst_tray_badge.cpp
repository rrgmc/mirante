// Tests the tray badge: its text, that it is painted at the icon's lower
// right in the attention red, and that a count of 0 leaves the icon alone.
#include "platform/TrayBadge.h"

#include <QGuiApplication>
#include <QPixmap>
#include <QTest>

using namespace mirante::platform;

class TestTrayBadge : public QObject
{
    Q_OBJECT

    static QImage blue(int size)
    {
        QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
        image.fill(QColor(0x20, 0x60, 0xc0));
        return image;
    }

    static bool isRed(QRgb pixel) { return qRed(pixel) > 180 && qGreen(pixel) < 120 && qBlue(pixel) < 120; }

private slots:
    void text()
    {
        QCOMPARE(trayBadgeText(1), QStringLiteral("1"));
        QCOMPARE(trayBadgeText(99), QStringLiteral("99"));
        QCOMPARE(trayBadgeText(100), QStringLiteral("99+"));
    }

    void zeroLeavesTheImage()
    {
        const QImage image = blue(16);
        QCOMPARE(paintTrayBadge(image, 0), image);
        QCOMPARE(paintTrayBadge(image, -1), image);
    }

    // The badge covers the lower right and leaves the upper left alone. Some
    // red shows in the badge, around the white number.
    void paintsLowerRight_data()
    {
        QTest::addColumn<int>("size");
        QTest::addColumn<int>("count");
        QTest::newRow("16, 3") << 16 << 3;
        QTest::newRow("32, 42") << 32 << 42;
        QTest::newRow("16, 99+") << 16 << 150;
    }

    void paintsLowerRight()
    {
        QFETCH(int, size);
        QFETCH(int, count);
        const QImage painted = paintTrayBadge(blue(size), count);
        QCOMPARE(painted.size(), QSize(size, size));
        QCOMPARE(painted.pixel(0, 0), blue(size).pixel(0, 0));
        bool red = false;
        for (int y = size / 2; y < size && !red; ++y) {
            for (int x = size / 2; x < size && !red; ++x)
                red = isRed(painted.pixel(x, y));
        }
        QVERIFY(red);
    }

    void iconHasEachTraySize()
    {
        QIcon icon;
        icon.addPixmap(QPixmap::fromImage(blue(64)));
        QCOMPARE(withTrayBadge(icon, 0).cacheKey(), icon.cacheKey());
        const QIcon badged = withTrayBadge(icon, 2);
        QVERIFY(badged.availableSizes().contains(QSize(16, 16)));
        QVERIFY(badged.availableSizes().contains(QSize(32, 32)));
    }
};

QTEST_MAIN(TestTrayBadge)
#include "tst_tray_badge.moc"
