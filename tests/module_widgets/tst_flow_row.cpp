// Tests FlowRow: wrapping, the height for a width, hidden widgets, a widget
// wider than the row, and that the row never asks for a minimum width.
#include <mirante/widgets/FlowRow.h>

#include <QLabel>
#include <QTest>
#include <QVBoxLayout>

using namespace mirante::widgets;

namespace {

constexpr int spacing = 10;

QWidget *addBox(QWidget *host, FlowRow *flow, int width, int height = 30)
{
    auto *box = new QWidget(host);
    box->setFixedSize(width, height);
    flow->addWidget(box);
    box->show();
    return box;
}

} // namespace

class TestFlowRow : public QObject
{
    Q_OBJECT

private slots:
    void staysOnOneRowWhenWide()
    {
        QWidget host;
        auto *flow = new FlowRow(&host);
        flow->setSpacing(spacing);
        QWidget *a = addBox(&host, flow, 100);
        QWidget *b = addBox(&host, flow, 100);

        flow->setGeometry(QRect(0, 0, 210, 100));
        QCOMPARE(a->geometry(), QRect(0, 0, 100, 30));
        QCOMPARE(b->geometry(), QRect(110, 0, 100, 30));
        QCOMPARE(flow->heightForWidth(210), 30);
    }

    void wrapsWhenTheWidthRunsOut()
    {
        QWidget host;
        auto *flow = new FlowRow(&host);
        flow->setSpacing(spacing);
        QWidget *a = addBox(&host, flow, 100);
        QWidget *b = addBox(&host, flow, 100);
        QWidget *c = addBox(&host, flow, 100);

        flow->setGeometry(QRect(0, 0, 209, 100));
        QCOMPARE(a->pos(), QPoint(0, 0));
        QCOMPARE(b->pos(), QPoint(0, 40));
        QCOMPARE(c->pos(), QPoint(0, 80));
        QCOMPARE(flow->heightForWidth(209), 110);

        flow->setGeometry(QRect(0, 0, 210, 100));
        QCOMPARE(b->pos(), QPoint(110, 0));
        QCOMPARE(c->pos(), QPoint(0, 40));
        QCOMPARE(flow->heightForWidth(210), 70);
    }

    void centresWidgetsInTheirRow()
    {
        QWidget host;
        auto *flow = new FlowRow(&host);
        flow->setSpacing(spacing);
        addBox(&host, flow, 50, 30);
        QWidget *small = addBox(&host, flow, 50, 10);

        flow->setGeometry(QRect(0, 0, 200, 100));
        QCOMPARE(small->y(), 10);
    }

    void skipsHiddenWidgets()
    {
        QWidget host;
        auto *flow = new FlowRow(&host);
        flow->setSpacing(spacing);
        QWidget *a = addBox(&host, flow, 100);
        QWidget *b = addBox(&host, flow, 100);
        QWidget *c = addBox(&host, flow, 100);
        b->hide();

        flow->setGeometry(QRect(0, 0, 210, 100));
        QCOMPARE(a->pos(), QPoint(0, 0));
        QCOMPARE(c->pos(), QPoint(110, 0));
        QCOMPARE(flow->heightForWidth(210), 30);

        a->hide();
        c->hide();
        QCOMPARE(flow->heightForWidth(210), 0);
    }

    void clampsAWidgetWiderThanTheRow()
    {
        QWidget host;
        auto *flow = new FlowRow(&host);
        auto *label = new QLabel(QStringLiteral("a rather long label for a narrow row"), &host);
        flow->addWidget(label);
        label->show();

        flow->setGeometry(QRect(0, 0, 40, 100));
        QCOMPARE(label->width(), 40);
    }

    void honoursAWrappingLabelsHeight()
    {
        QWidget host;
        auto *flow = new FlowRow(&host);
        auto *label = new QLabel(QStringLiteral("some words that need several lines here"), &host);
        label->setWordWrap(true);
        flow->addWidget(label);
        label->show();

        QVERIFY(flow->heightForWidth(60) > flow->heightForWidth(600));
    }

    void hasNoMinimumWidth()
    {
        QWidget host;
        auto *flow = new FlowRow(&host);
        addBox(&host, flow, 300);
        QCOMPARE(flow->minimumSize().width(), 0);
        QCOMPARE(flow->minimumSize().height(), 30);
        QCOMPARE(flow->sizeHint(), QSize(300, 30));
    }

    void honoursMargins()
    {
        QWidget host;
        auto *flow = new FlowRow(&host);
        flow->setContentsMargins(5, 6, 7, 8);
        QWidget *a = addBox(&host, flow, 100);

        flow->setGeometry(QRect(0, 0, 200, 100));
        QCOMPARE(a->pos(), QPoint(5, 6));
        QCOMPARE(flow->heightForWidth(200), 44);
    }

    void aParentLayoutGivesTheHeightItNeeds()
    {
        QWidget window;
        auto *outer = new QVBoxLayout(&window);
        outer->setContentsMargins(0, 0, 0, 0);
        auto *row = new QWidget(&window);
        auto *flow = new FlowRow(row);
        flow->setSpacing(spacing);
        QWidget *last = nullptr;
        for (int i = 0; i < 4; ++i)
            last = addBox(row, flow, 100);
        outer->addWidget(row);
        outer->addStretch(1);

        window.resize(120, 400);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTRY_COMPARE(row->height(), 150);
        QCOMPARE(last->y(), 120);
    }
};

QTEST_MAIN(TestFlowRow)
#include "tst_flow_row.moc"
