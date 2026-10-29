// Tests PaintedIconEngine: a pixmap of a painted icon has no stray pixels
// around its shape, at the normal scale and at a high-DPI scale.
#include "items/PaintedIconEngine.h"

#include <QIcon>
#include <QImage>
#include <QTest>

using namespace mirante;

namespace {

// A small red dot in the middle, leaving the corners empty.
class DotEngine : public PaintedIconEngine
{
public:
    void paint(QPainter *painter, const QRect &rect, QIcon::Mode, QIcon::State) override
    {
        painter->fillRect(QRectF(rect).adjusted(rect.width() / 4.0, rect.height() / 4.0, -rect.width() / 4.0,
                                                -rect.height() / 4.0),
                          Qt::red);
    }

    QIconEngine *clone() const override { return new DotEngine; }
};

bool cornersClear(const QImage &image)
{
    const int right = image.width() - 1;
    const int bottom = image.height() - 1;
    for (const QPoint &corner : {QPoint(0, 0), QPoint(right, 0), QPoint(0, bottom), QPoint(right, bottom)}) {
        if (qAlpha(image.pixel(corner)) != 0)
            return false;
    }
    return true;
}

} // namespace

class TestPaintedIcon : public QObject
{
    Q_OBJECT

private slots:
    void pixmapStartsTransparent()
    {
        const QIcon icon(new DotEngine);
        const QImage image = icon.pixmap(QSize(32, 32)).toImage();
        QCOMPARE(image.size(), QSize(32, 32));
        QVERIFY(cornersClear(image));
        QCOMPARE(QColor(image.pixel(16, 16)), QColor(Qt::red));
    }

    void scaledPixmapKeepsItsScale()
    {
        DotEngine engine;
        const QPixmap pixmap = engine.scaledPixmap(QSize(20, 20), QIcon::Normal, QIcon::Off, 2.0);
        QCOMPARE(pixmap.size(), QSize(40, 40));
        QCOMPARE(pixmap.devicePixelRatio(), 2.0);
        const QImage image = pixmap.toImage();
        QVERIFY(cornersClear(image));
        QCOMPARE(QColor(image.pixel(20, 20)), QColor(Qt::red));
    }
};

QTEST_MAIN(TestPaintedIcon)
#include "tst_painted_icon.moc"
