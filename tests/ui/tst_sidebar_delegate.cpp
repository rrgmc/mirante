// Tests SidebarDelegate's priority marker: projects get a rounded square,
// tasks a circle, in the same colour for the same priority.
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
};

QTEST_MAIN(TestSidebarDelegate)
#include "tst_sidebar_delegate.moc"
