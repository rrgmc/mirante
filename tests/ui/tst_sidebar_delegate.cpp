// Tests SidebarDelegate's priority marker: projects get a rounded square,
// tasks a circle, in the same colour for the same priority. Rows with an
// attention count get a badge at their right.
#include "core/Entities.h"
#include "ui/SidebarDelegate.h"
#include "ui/SidebarModel.h"

#include <QPainter>
#include <QStandardItemModel>
#include <QStyleOptionViewItem>
#include <QTest>

using namespace mirante;

namespace {

// Exposes initStyleOption, which sets the marker as the row's icon.
class Delegate : public SidebarDelegate
{
public:
    using SidebarDelegate::initStyleOption;
};

} // namespace

class TestSidebarDelegate : public QObject
{
    Q_OBJECT

    // The marker of a row of this kind and priority, painted at 64 x 64 pixels.
    static QImage marker(SidebarModel::Kind kind, Priority priority)
    {
        QStandardItemModel model;
        auto *item = new QStandardItem(QStringLiteral("row"));
        item->setData(int(kind), SidebarModel::KindRole);
        item->setData(int(priority), SidebarModel::PriorityRole);
        model.appendRow(item);

        Delegate delegate;
        QStyleOptionViewItem option;
        delegate.initStyleOption(&option, model.index(0, 0));
        if (option.icon.isNull())
            return QImage();

        // Painted into an image with alpha, as pixmaps on some platforms have none.
        QImage image(64, 64, QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        option.icon.paint(&painter, image.rect());
        painter.end();
        return image;
    }

private slots:
    void shapeByKind()
    {
        QCOMPARE(SidebarDelegate::markerShape(SidebarModel::Kind::Project),
                 SidebarDelegate::MarkerShape::RoundedSquare);
        QCOMPARE(SidebarDelegate::markerShape(SidebarModel::Kind::Task), SidebarDelegate::MarkerShape::Circle);
    }

    // The marker covers the middle 32 x 32 pixels. Pixel (19, 19) lies inside
    // the rounded square's corner but outside the circle.
    void paintedShape()
    {
        const QImage project = marker(SidebarModel::Kind::Project, Priority::Urgent);
        const QImage task = marker(SidebarModel::Kind::Task, Priority::Urgent);
        const QRgb urgent = QColor(0xd9, 0x3b, 0x3b).rgb();

        QCOMPARE(project.pixelColor(32, 32).rgb(), urgent);
        QCOMPARE(task.pixelColor(32, 32).rgb(), urgent);

        QCOMPARE(project.pixelColor(19, 19).alpha(), 255);
        QCOMPARE(task.pixelColor(19, 19).alpha(), 0);

        // Outside the marker, both are clear.
        QCOMPARE(project.pixelColor(10, 10).alpha(), 0);
        QCOMPARE(task.pixelColor(10, 10).alpha(), 0);
    }

    void otherRowsHaveNoMarker()
    {
        QVERIFY(marker(SidebarModel::Kind::Group, Priority::Urgent).isNull());
    }

    // A row with an attention count gets a red badge at its right. The text
    // keeps clear of it, and the row asks for its width.
    void attentionBadge()
    {
        const QRgb attention = QColor(0xd9, 0x48, 0x3b).rgb();
        for (const auto kind : {SidebarModel::Kind::Group, SidebarModel::Kind::Project, SidebarModel::Kind::Task}) {
            QStandardItemModel model;
            auto *plain = new QStandardItem(QStringLiteral("row"));
            plain->setData(int(kind), SidebarModel::KindRole);
            plain->setData(int(Priority::Normal), SidebarModel::PriorityRole);
            auto *badged = plain->clone();
            badged->setData(3, SidebarModel::AttentionRole);
            model.appendRow(plain);
            model.appendRow(badged);

            Delegate delegate;
            const QRect rect(0, 0, 200, 24);
            const auto paint = [&](const QModelIndex &index) {
                QImage image(rect.size(), QImage::Format_ARGB32);
                image.fill(Qt::white);
                QStyleOptionViewItem option;
                option.rect = rect;
                QPainter painter(&image);
                delegate.paint(&painter, option, index);
                painter.end();
                return image;
            };
            const auto attentionPixels = [attention](const QImage &image, int from, int to) {
                int pixels = 0;
                for (int y = 0; y < image.height(); ++y) {
                    for (int x = from; x < to; ++x)
                        pixels += image.pixel(x, y) == attention ? 1 : 0;
                }
                return pixels;
            };

            const QImage withBadge = paint(model.index(1, 0));
            QVERIFY(attentionPixels(withBadge, 160, 200) > 40);
            QCOMPARE(attentionPixels(withBadge, 0, 160), 0);
            QCOMPARE(attentionPixels(paint(model.index(0, 0)), 0, 200), 0);

            QStyleOptionViewItem option;
            option.rect = rect;
            delegate.initStyleOption(&option, model.index(1, 0));
            QVERIFY(option.rect.right() < 180);
            QVERIFY(delegate.sizeHint(option, model.index(1, 0)).width()
                    > delegate.sizeHint(option, model.index(0, 0)).width());
        }
    }
};

QTEST_MAIN(TestSidebarDelegate)
#include "tst_sidebar_delegate.moc"
