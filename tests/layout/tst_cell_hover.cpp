// Tests CellHoverTracker: the innermost hovered cell wins, and leaving it
// gives the hover back to the cell around it.
#include "layout/CellHoverTracker.h"

#include <QEnterEvent>
#include <QSignalSpy>
#include <QTest>
#include <QWidget>

#include <memory>

using namespace mirante;

class TestCellHover : public QObject
{
    Q_OBJECT

private slots:
    void nestedCells();
    void deletedAndHiddenCells();
};

namespace {

void enter(QWidget *widget)
{
    QEnterEvent event(QPointF(1, 1), QPointF(1, 1), QPointF(1, 1));
    QCoreApplication::sendEvent(widget, &event);
}

void leave(QWidget *widget)
{
    QEvent event(QEvent::Leave);
    QCoreApplication::sendEvent(widget, &event);
}

} // namespace

void TestCellHover::nestedCells()
{
    CellHoverTracker tracker;
    QSignalSpy spy(&tracker, &CellHoverTracker::hoveredChanged);
    QWidget task;
    auto *item = new QWidget(&task);
    tracker.watch(&task, QStringLiteral("task"));
    tracker.watch(item, QStringLiteral("item"));
    QVERIFY(tracker.hoveredId().isEmpty());

    // Qt sends Enter to the parent first, then to the child.
    enter(&task);
    QCOMPARE(tracker.hoveredId(), QStringLiteral("task"));
    enter(item);
    QCOMPARE(tracker.hoveredId(), QStringLiteral("item"));

    // Back to the task cell's own area, then out of it.
    leave(item);
    QCOMPARE(tracker.hoveredId(), QStringLiteral("task"));
    leave(&task);
    QVERIFY(tracker.hoveredId().isEmpty());

    QCOMPARE(spy.count(), 4);
    QCOMPARE(spy.at(1).at(0).toString(), QStringLiteral("item"));
    QVERIFY(spy.at(3).at(0).toString().isEmpty());
}

void TestCellHover::deletedAndHiddenCells()
{
    CellHoverTracker tracker;
    auto cell = std::make_unique<QWidget>();
    tracker.watch(cell.get(), QStringLiteral("item"));
    enter(cell.get());
    QCOMPARE(tracker.hoveredId(), QStringLiteral("item"));

    // A cell rebuilt by a layout change gets no Leave.
    cell.reset();
    QVERIFY(tracker.hoveredId().isEmpty());

    // A cell on a tab that is no longer shown is not hovered.
    QWidget other;
    other.show();
    tracker.watch(&other, QStringLiteral("other"));
    enter(&other);
    QCOMPARE(tracker.hoveredId(), QStringLiteral("other"));
    other.hide();
    QVERIFY(tracker.hoveredId().isEmpty());
}

QTEST_MAIN(TestCellHover)
#include "tst_cell_hover.moc"
