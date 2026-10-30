// Tests CellFrame's status area: each kind of CellStatus shows its own
// widget, the problem pill with a fix emits fixRequested, and the refresh
// button shows when asked and emits refreshRequested. Also tests the
// drops of images from other apps that setImageDrop() turns on.
#include "layout/CellFrame.h"

#include <QApplication>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMimeData>
#include <QSignalSpy>
#include <QTest>
#include <QToolButton>

#include <memory>

using namespace mirante;

namespace {

std::unique_ptr<CellFrame> itemFrame()
{
    CellRef ref;
    ref.task = QStringLiteral("t1");
    ref.item = QStringLiteral("i1");
    auto frame = std::make_unique<CellFrame>(ref, false, nullptr, QStringLiteral("Item"));
    frame->resize(400, 200);
    return frame;
}

// The widgets of the status area that show.
QStringList shown(const CellFrame &frame)
{
    QStringList names;
    for (const char *name : {"attentionBadge", "problemPill", "problemButton", "staleText"}) {
        const auto *widget = frame.findChild<QWidget *>(QLatin1String(name));
        if (widget && widget->isVisibleTo(&frame))
            names << QLatin1String(name);
    }
    return names;
}

CellStatus status(CellStatus::Kind kind, const QString &text = {}, int count = 0, bool canFix = false)
{
    CellStatus s;
    s.kind = kind;
    s.text = text;
    s.count = count;
    s.canFix = canFix;
    s.tooltip = QStringLiteral("why");
    return s;
}

// Sends a drag over target and drops it there, as a drag from another app
// does. Returns the action of an accepted drop, or IgnoreAction for a drag
// refused at any step.
Qt::DropAction dragAndDrop(QWidget *target, const QMimeData *mime,
                           Qt::DropActions actions = Qt::CopyAction | Qt::MoveAction)
{
    const QPoint pos = target->rect().center();
    QDragEnterEvent enter(pos, actions, mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(target, &enter);
    if (!enter.isAccepted())
        return Qt::IgnoreAction;
    QDragMoveEvent move(pos, actions, mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(target, &move);
    if (!move.isAccepted()) {
        QDragLeaveEvent leave;
        QApplication::sendEvent(target, &leave);
        return Qt::IgnoreAction;
    }
    QDropEvent drop(QPointF(pos), actions, mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(target, &drop);
    return drop.isAccepted() ? drop.dropAction() : Qt::IgnoreAction;
}

QImage redImage()
{
    QImage image(8, 6, QImage::Format_RGB32);
    image.fill(Qt::red);
    return image;
}

} // namespace

class TestCellFrame : public QObject
{
    Q_OBJECT

private slots:
    // A cell takes no drops of its own until setImageDrop().
    void noImageDropByDefault()
    {
        auto frame = itemFrame();
        QVERIFY(!frame->acceptDrops());
        QMimeData mime;
        mime.setImageData(redImage());
        QCOMPARE(dragAndDrop(frame.get(), &mime), Qt::IgnoreAction);
    }

    // An image the reader reads is dropped as a copy, even when the source
    // proposes a move, and reaches the callback once the drop is over.
    void imageDrop()
    {
        auto frame = itemFrame();
        QList<QImage> dropped;
        frame->setImageDrop([](const QMimeData *data) { return qvariant_cast<QImage>(data->imageData()); },
                            [&dropped](const QImage &image) { dropped << image; });
        QVERIFY(frame->acceptDrops());
        QMimeData mime;
        mime.setImageData(redImage());
        QCOMPARE(dragAndDrop(frame.get(), &mime), Qt::CopyAction);
        QVERIFY(dropped.isEmpty());
        QTRY_COMPARE(dropped.size(), 1);
        QCOMPARE(dropped.front().size(), QSize(8, 6));
    }

    // A drag the reader finds no image in is refused, and so is one that
    // cannot be copied.
    void refusedDrops()
    {
        auto frame = itemFrame();
        int dropped = 0;
        frame->setImageDrop([](const QMimeData *data) { return qvariant_cast<QImage>(data->imageData()); },
                            [&dropped](const QImage &) { ++dropped; });
        QMimeData text;
        text.setText(QStringLiteral("not an image"));
        QCOMPARE(dragAndDrop(frame.get(), &text), Qt::IgnoreAction);
        QMimeData image;
        image.setImageData(redImage());
        QCOMPARE(dragAndDrop(frame.get(), &image, Qt::MoveAction), Qt::IgnoreAction);
        QCoreApplication::processEvents();
        QCOMPARE(dropped, 0);
    }

    void noStatusByDefault()
    {
        auto frame = itemFrame();
        QVERIFY(shown(*frame).isEmpty());
        QVERIFY(frame->status() == CellStatus());
        auto *refresh = frame->findChild<QToolButton *>(QStringLiteral("refreshButton"));
        QVERIFY(refresh);
        QVERIFY(!refresh->isVisibleTo(frame.get()));
    }

    void attentionBadge()
    {
        auto frame = itemFrame();
        const CellStatus attention = status(CellStatus::Kind::Attention, {}, 2);
        frame->setStatus(attention);
        QVERIFY(frame->status() == attention);
        QCOMPARE(shown(*frame), QStringList{QStringLiteral("attentionBadge")});
        auto *badge = frame->findChild<QLabel *>(QStringLiteral("attentionBadge"));
        QCOMPARE(badge->text(), QStringLiteral("2"));
        QCOMPARE(badge->toolTip(), QStringLiteral("why"));

        frame->setStatus(status(CellStatus::Kind::Attention, {}, 120));
        QCOMPARE(badge->text(), QStringLiteral("99+"));
    }

    // A problem the user can fix is a button; one without a fix is a label.
    void problemPill()
    {
        auto frame = itemFrame();
        QSignalSpy fix(frame.get(), &CellFrame::fixRequested);

        frame->setStatus(status(CellStatus::Kind::Problem, QStringLiteral("Not found")));
        QCOMPARE(shown(*frame), QStringList{QStringLiteral("problemPill")});
        auto *pill = frame->findChild<QLabel *>(QStringLiteral("problemPill"));
        QCOMPARE(pill->text(), QStringLiteral("Not found"));
        QCOMPARE(pill->toolTip(), QStringLiteral("why"));

        frame->setStatus(status(CellStatus::Kind::Problem, QStringLiteral("Token not set"), 0, true));
        QCOMPARE(shown(*frame), QStringList{QStringLiteral("problemButton")});
        auto *button = frame->findChild<QToolButton *>(QStringLiteral("problemButton"));
        QCOMPARE(button->text(), QStringLiteral("Token not set"));
        QCOMPARE(button->toolTip(), QStringLiteral("why"));
        QCOMPARE(fix.count(), 0);
        button->click();
        QCOMPARE(fix.count(), 1);
    }

    void staleText()
    {
        auto frame = itemFrame();
        frame->setStatus(status(CellStatus::Kind::Stale, QStringLiteral("12 min ago")));
        QCOMPARE(shown(*frame), QStringList{QStringLiteral("staleText")});
        auto *text = frame->findChild<QLabel *>(QStringLiteral("staleText"));
        QCOMPARE(text->text(), QStringLiteral("12 min ago"));
        QCOMPARE(text->foregroundRole(), QPalette::PlaceholderText);

        // Back to none hides it.
        frame->setStatus({});
        QVERIFY(shown(*frame).isEmpty());
    }

    void refreshButton()
    {
        auto frame = itemFrame();
        QSignalSpy refresh(frame.get(), &CellFrame::refreshRequested);
        auto *button = frame->findChild<QToolButton *>(QStringLiteral("refreshButton"));
        frame->setRefreshVisible(true);
        QVERIFY(button->isVisibleTo(frame.get()));
        QVERIFY(!button->icon().isNull());
        button->click();
        QCOMPARE(refresh.count(), 1);
        frame->setRefreshVisible(false);
        QVERIFY(!button->isVisibleTo(frame.get()));
    }

    // The status area comes after a header widget and before the menu
    // button: header row, header widget, status, refresh, menu, maximise.
    void order()
    {
        auto frame = itemFrame();
        QWidget toolbar;
        frame->setHeaderWidget(&toolbar);
        frame->setStatus(status(CellStatus::Kind::Attention, {}, 1));
        frame->setRefreshVisible(true);

        auto *header = frame->findChild<QWidget *>(QStringLiteral("cellHeader"));
        auto *layout = qobject_cast<QHBoxLayout *>(header->layout());
        QVERIFY(layout);
        const auto position = [layout](QWidget *widget) { return layout->indexOf(widget); };
        auto *badge = frame->findChild<QWidget *>(QStringLiteral("attentionBadge"));
        auto *refresh = frame->findChild<QWidget *>(QStringLiteral("refreshButton"));
        auto *maximise = frame->findChild<QWidget *>(QStringLiteral("maximiseButton"));
        QVERIFY(position(&toolbar) < position(badge));
        QVERIFY(position(badge) < position(refresh));
        QCOMPARE(position(refresh) + 2, position(maximise));   // the menu button is between
        frame->setHeaderWidget(nullptr);
    }
};

QTEST_MAIN(TestCellFrame)
#include "tst_cell_frame.moc"
