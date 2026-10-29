// Tests the crop maths of ImageCropView (fitting, mapping between the scaled
// preview and the image, handles, clamping), the view under the mouse, and
// ImagePasteDialog: the size text, Whole Image, the cropped image at full
// resolution, and Enter and Escape.
#include "ui/ImageCropView.h"
#include "ui/ImagePasteDialog.h"

#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>

using namespace mirante;
using imagecrop::Handle;

namespace {

// An image whose pixels say where they are: red is x, green is y.
QImage gradient(int width, int height)
{
    QImage image(width, height, QImage::Format_RGB32);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x)
            image.setPixel(x, y, qRgb(x % 256, y % 256, 0));
    }
    return image;
}

} // namespace

class TestImageCrop : public QObject
{
    Q_OBJECT

private slots:
    // A large image shrinks to fit, keeping its shape, centred; a small one
    // keeps its own size.
    void fit()
    {
        QCOMPARE(imagecrop::fit(QSize(2000, 1000), QRectF(0, 0, 400, 400)), QRectF(0, 100, 400, 200));
        QCOMPARE(imagecrop::fit(QSize(1000, 2000), QRectF(10, 10, 400, 400)), QRectF(110, 10, 200, 400));
        QCOMPARE(imagecrop::fit(QSize(100, 50), QRectF(0, 0, 400, 400)), QRectF(150, 175, 100, 50));
        QVERIFY(imagecrop::fit(QSize(), QRectF(0, 0, 400, 400)).isEmpty());
    }

    // A point of the preview maps to the nearest pixel boundary of the full
    // image, clamped to it; a rectangle of the image maps back to the preview.
    void mapping()
    {
        const QSize image(2000, 1000);
        const QRectF shown(0, 100, 400, 200);   // shown at a fifth of its size
        QCOMPARE(imagecrop::toImage(QPointF(0, 100), shown, image), QPoint(0, 0));
        QCOMPARE(imagecrop::toImage(QPointF(100, 150), shown, image), QPoint(500, 250));
        QCOMPARE(imagecrop::toImage(QPointF(100.3, 150.2), shown, image), QPoint(502, 251));
        QCOMPARE(imagecrop::toImage(QPointF(400, 300), shown, image), QPoint(2000, 1000));
        QCOMPARE(imagecrop::toImage(QPointF(-50, 0), shown, image), QPoint(0, 0));
        QCOMPARE(imagecrop::toImage(QPointF(900, 900), shown, image), QPoint(2000, 1000));

        QCOMPARE(imagecrop::toView(QRect(500, 250, 1000, 500), shown, image), QRectF(100, 150, 200, 100));
        QCOMPARE(imagecrop::toView(QRect(0, 0, 2000, 1000), shown, image), shown);
    }

    // Corners come before edges, edges before the inside; nothing outside.
    void handles()
    {
        const QRectF rect(100, 100, 200, 100);
        const qreal grab = 6;
        QCOMPARE(imagecrop::handleAt(rect, QPointF(100, 100), grab), Handle::TopLeft);
        QCOMPARE(imagecrop::handleAt(rect, QPointF(304, 96), grab), Handle::TopRight);
        QCOMPARE(imagecrop::handleAt(rect, QPointF(98, 203), grab), Handle::BottomLeft);
        QCOMPARE(imagecrop::handleAt(rect, QPointF(300, 200), grab), Handle::BottomRight);
        QCOMPARE(imagecrop::handleAt(rect, QPointF(103, 150), grab), Handle::Left);
        QCOMPARE(imagecrop::handleAt(rect, QPointF(297, 150), grab), Handle::Right);
        QCOMPARE(imagecrop::handleAt(rect, QPointF(200, 95), grab), Handle::Top);
        QCOMPARE(imagecrop::handleAt(rect, QPointF(200, 205), grab), Handle::Bottom);
        QCOMPARE(imagecrop::handleAt(rect, QPointF(200, 150), grab), Handle::Move);
        QCOMPARE(imagecrop::handleAt(rect, QPointF(200, 220), grab), Handle::None);
        QCOMPARE(imagecrop::handleAt(rect, QPointF(50, 150), grab), Handle::None);
        QCOMPARE(imagecrop::handleAt(QRectF(), QPointF(0, 0), grab), Handle::None);
        // In a crop narrower than two grabs, the nearer edge wins.
        QCOMPARE(imagecrop::handleAt(QRectF(100, 100, 8, 100), QPointF(101, 150), grab), Handle::Left);
        QCOMPARE(imagecrop::handleAt(QRectF(100, 100, 8, 100), QPointF(107, 150), grab), Handle::Right);
    }

    void spanned()
    {
        const QSize image(100, 50);
        QCOMPARE(imagecrop::spanned(QPoint(10, 10), QPoint(30, 40), image), QRect(10, 10, 20, 30));
        QCOMPARE(imagecrop::spanned(QPoint(30, 40), QPoint(10, 10), image), QRect(10, 10, 20, 30));
        QCOMPARE(imagecrop::spanned(QPoint(-10, -10), QPoint(500, 500), image), QRect(0, 0, 100, 50));
        QVERIFY(imagecrop::spanned(QPoint(10, 10), QPoint(10, 40), image).isEmpty());
    }

    // Moving keeps the size and stops at the image's edges. Resizing moves
    // only the handle's edges, stops at the edges, turns over past the
    // opposite edge, and keeps at least one pixel.
    void dragged()
    {
        const QSize image(100, 50);
        const QRect start(10, 10, 20, 20);
        QCOMPARE(imagecrop::dragged(start, Handle::Move, QPoint(5, 5), image), QRect(15, 15, 20, 20));
        QCOMPARE(imagecrop::dragged(start, Handle::Move, QPoint(500, 500), image), QRect(80, 30, 20, 20));
        QCOMPARE(imagecrop::dragged(start, Handle::Move, QPoint(-500, -500), image), QRect(0, 0, 20, 20));

        QCOMPARE(imagecrop::dragged(start, Handle::Left, QPoint(-5, 99), image), QRect(5, 10, 25, 20));
        QCOMPARE(imagecrop::dragged(start, Handle::Right, QPoint(500, 0), image), QRect(10, 10, 90, 20));
        QCOMPARE(imagecrop::dragged(start, Handle::Top, QPoint(0, -500), image), QRect(10, 0, 20, 30));
        QCOMPARE(imagecrop::dragged(start, Handle::Bottom, QPoint(0, 5), image), QRect(10, 10, 20, 25));
        QCOMPARE(imagecrop::dragged(start, Handle::TopLeft, QPoint(5, 5), image), QRect(15, 15, 15, 15));
        QCOMPARE(imagecrop::dragged(start, Handle::BottomRight, QPoint(-5, 5), image), QRect(10, 10, 15, 25));
        QCOMPARE(imagecrop::dragged(start, Handle::TopRight, QPoint(1, 1), image), QRect(10, 11, 21, 19));
        QCOMPARE(imagecrop::dragged(start, Handle::BottomLeft, QPoint(1, 1), image), QRect(11, 10, 19, 21));

        // Past the opposite edge, the rectangle turns over.
        QCOMPARE(imagecrop::dragged(start, Handle::Left, QPoint(30, 0), image), QRect(30, 10, 10, 20));
        // Onto the opposite edge, one pixel is kept.
        QCOMPARE(imagecrop::dragged(start, Handle::Left, QPoint(20, 0), image), QRect(30, 10, 1, 20));
        QCOMPARE(imagecrop::dragged(QRect(90, 10, 10, 20), Handle::Left, QPoint(10, 0), image),
                 QRect(99, 10, 1, 20));
        QCOMPARE(imagecrop::dragged(start, Handle::None, QPoint(5, 5), image), start);
    }

    // setCrop() clamps to the image; an empty crop or the whole image is no crop.
    void viewCrop()
    {
        ImageCropView view;
        view.setImage(gradient(100, 50));
        QSignalSpy changed(&view, &ImageCropView::cropChanged);
        QVERIFY(view.crop().isEmpty());
        QCOMPARE(view.selection(), QRect(0, 0, 100, 50));

        view.setCrop(QRect(80, 40, 50, 50));
        QCOMPARE(view.crop(), QRect(80, 40, 20, 10));
        QCOMPARE(view.selection(), QRect(80, 40, 20, 10));
        QCOMPARE(changed.count(), 1);

        view.setCrop(QRect(-5, -5, 200, 200));
        QVERIFY(view.crop().isEmpty());
        view.setCrop(QRect(10, 10, 20, 20));
        view.clearCrop();
        QVERIFY(view.crop().isEmpty());
        QCOMPARE(changed.count(), 4);

        view.setCrop(QRect(10, 10, 20, 20));
        view.setImage(gradient(10, 10));
        QVERIFY(view.crop().isEmpty());
    }

    // The mouse draws a crop on the scaled preview, in image pixels, then
    // moves it and resizes it by a handle. A click outside removes it.
    void viewMouse()
    {
        ImageCropView view;
        view.setImage(gradient(800, 400));
        view.resize(416, 416);     // the image shows at half size: 400 × 200, at (8, 108)
        QCOMPARE(view.shownRect(), QRectF(8, 108, 400, 200));

        QTest::mousePress(&view, Qt::LeftButton, {}, QPoint(58, 158));
        QTest::mouseMove(&view, QPoint(158, 208));
        QTest::mouseRelease(&view, Qt::LeftButton, {}, QPoint(158, 208));
        QCOMPARE(view.crop(), QRect(100, 100, 200, 100));

        // Dragging inside moves it, but not past the image's edge.
        QTest::mousePress(&view, Qt::LeftButton, {}, QPoint(100, 180));
        QTest::mouseMove(&view, QPoint(0, 180));
        QTest::mouseRelease(&view, Qt::LeftButton, {}, QPoint(0, 180));
        QCOMPARE(view.crop(), QRect(0, 100, 200, 100));

        // The bottom-right corner, at (108, 208), resizes it, up to the
        // image's bottom edge.
        QTest::mousePress(&view, Qt::LeftButton, {}, QPoint(108, 208));
        QTest::mouseMove(&view, QPoint(308, 400));
        QTest::mouseRelease(&view, Qt::LeftButton, {}, QPoint(308, 400));
        QCOMPARE(view.crop(), QRect(0, 100, 600, 300));

        // A click outside it removes it.
        QTest::mouseClick(&view, Qt::LeftButton, {}, QPoint(380, 130));
        QVERIFY(view.crop().isEmpty());
    }

    // The dialog shows the sizes, crops at full resolution, and Whole Image
    // removes the crop.
    void dialog()
    {
        ImagePasteDialog dialog(gradient(300, 200));
        auto *size = dialog.findChild<QLabel *>(QStringLiteral("imageSize"));
        auto *whole = dialog.findChild<QPushButton *>(QStringLiteral("wholeImage"));
        QVERIFY(size && whole && dialog.cropView());
        QCOMPARE(size->text(), QStringLiteral("300 × 200 pixels"));
        QVERIFY(!whole->isEnabled());
        QCOMPARE(dialog.croppedImage().size(), QSize(300, 200));
        // A longer size text does not make the dialog wider.
        const int width = dialog.minimumSizeHint().width();

        dialog.setCrop(QRect(250, 150, 40, 30));
        QCOMPARE(dialog.minimumSizeHint().width(), width);
        dialog.setCrop(QRect(10, 10, 280, 180));
        QCOMPARE(dialog.minimumSizeHint().width(), width);
        dialog.setCrop(QRect(250, 150, 40, 30));
        QCOMPARE(size->text(), QStringLiteral("Crop: 40 × 30 of 300 × 200 pixels"));
        QVERIFY(whole->isEnabled());
        const QImage cropped = dialog.croppedImage();
        QCOMPARE(cropped.size(), QSize(40, 30));
        QCOMPARE(cropped.pixel(0, 0), qRgb(250, 150, 0));
        QCOMPARE(cropped.pixel(39, 29), qRgb(289 % 256, 179, 0));

        whole->click();
        QVERIFY(dialog.crop().isEmpty());
        QCOMPARE(size->text(), QStringLiteral("300 × 200 pixels"));
        QCOMPARE(dialog.croppedImage().size(), QSize(300, 200));
    }

    // ask() returns the cropped image when accepted, and a null image when not.
    void ask()
    {
        const QImage image = gradient(60, 40);
        const QImage cropped = ImagePasteDialog::ask(image, nullptr, [](QDialog &d) {
            auto *dialog = qobject_cast<ImagePasteDialog *>(&d);
            if (!dialog)
                return false;
            dialog->setCrop(QRect(10, 5, 20, 10));
            return true;
        });
        QCOMPARE(cropped, image.copy(10, 5, 20, 10));
        QVERIFY(ImagePasteDialog::ask(image, nullptr, [](QDialog &) { return false; }).isNull());
    }

    // Enter pastes and Escape cancels, also with the mouse over the preview.
    void keys()
    {
        for (const auto &[key, expected] : {std::pair{Qt::Key_Return, int(QDialog::Accepted)},
                                            std::pair{Qt::Key_Enter, int(QDialog::Accepted)},
                                            std::pair{Qt::Key_Escape, int(QDialog::Rejected)}}) {
            ImagePasteDialog dialog(gradient(60, 40));
            QTimer::singleShot(0, &dialog, [&dialog, key = key] {
                QWidget *target = dialog.focusWidget() ? dialog.focusWidget() : &dialog;
                QTest::keyClick(target, key);
            });
            QCOMPARE(dialog.exec(), expected);
        }
    }
};

QTEST_MAIN(TestImageCrop)
#include "tst_image_crop.moc"
