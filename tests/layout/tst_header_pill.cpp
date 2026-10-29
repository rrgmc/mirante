// Tests of HeaderPill and HeaderRow: the parts of a cell header and the order
// in which they give way in a narrow cell.
#include "layout/HeaderPill.h"

#include <QPixmap>
#include <QTest>

using namespace mirante;

class TestHeaderPill : public QObject
{
    Q_OBJECT

private slots:
    void blankPartsTakeNoSpace();
    void shrinkOrder();
    void typePillKeepsItsIcon();
    void moduleHues();
};

namespace {

QIcon someIcon()
{
    QPixmap pixmap(16, 16);
    pixmap.fill(Qt::darkGray);
    return QIcon(pixmap);
}

struct Header
{
    HeaderRow row;
    HeaderPill *type = new HeaderPill;
    HeaderPill *name = new HeaderPill;
    HeaderPill *place = new HeaderPill;

    Header()
    {
        type->setFill(HeaderPill::Fill::Module, QStringLiteral("gitlab"));
        type->setIcon(someIcon());
        type->setText(QStringLiteral("MR"));
        name->setText(QStringLiteral("!228 Token bucket limiter"));
        place->setFill(HeaderPill::Fill::Neutral);
        place->setText(QStringLiteral("backend › Add rate limiting"));
        row.setParts({type, name, place});
        row.setShrinkOrder({place, name, type});
    }

    void setWidth(int width)
    {
        row.resize(width, row.sizeHint().height());
        row.refresh();
    }
};

} // namespace

void TestHeaderPill::blankPartsTakeNoSpace()
{
    Header header;
    const int full = header.row.sizeHint().width();
    header.place->clear();
    header.row.refresh();
    QVERIFY(header.row.sizeHint().width() < full);
    header.setWidth(full);
    QVERIFY(header.place->isHidden());
    QVERIFY(!header.name->isHidden());
}

// The place gives way first, then the name, then the type's label.
void TestHeaderPill::shrinkOrder()
{
    Header header;
    const int full = header.row.sizeHint().width();
    header.setWidth(full);
    QCOMPARE(header.place->width(), header.place->sizeHint().width());
    QCOMPARE(header.name->width(), header.name->sizeHint().width());

    header.setWidth(full - 20);
    QCOMPARE(header.place->width(), header.place->sizeHint().width() - 20);
    QCOMPARE(header.name->width(), header.name->sizeHint().width());
    QCOMPARE(header.type->width(), header.type->sizeHint().width());

    const int placeMinimum = header.place->minimumSizeHint().width();
    header.setWidth(full - (header.place->sizeHint().width() - placeMinimum) - 10);
    QCOMPARE(header.place->width(), placeMinimum);
    QCOMPARE(header.name->width(), header.name->sizeHint().width() - 10);
    QCOMPARE(header.type->width(), header.type->sizeHint().width());
}

void TestHeaderPill::typePillKeepsItsIcon()
{
    Header header;
    QCOMPARE(header.row.minimumSizeHint().width(), header.type->minimumSizeHint().width());
    QVERIFY(header.type->minimumSizeHint().width() > 0);
    QVERIFY(header.type->minimumSizeHint().width() < header.type->sizeHint().width());

    header.setWidth(header.type->minimumSizeHint().width());
    QCOMPARE(header.type->width(), header.type->minimumSizeHint().width());
    QCOMPARE(header.type->geometry().left(), 0);
}

void TestHeaderPill::moduleHues()
{
    QCOMPARE(HeaderPill::moduleHue(QStringLiteral("gitlab")), 25);
    QCOMPARE(HeaderPill::moduleHue(QStringLiteral("jira")), 215);
    const int other = HeaderPill::moduleHue(QStringLiteral("timetrack"));
    QVERIFY(other >= 0 && other < 360);
    QCOMPARE(HeaderPill::moduleHue(QStringLiteral("timetrack")), other);

    // A pill's colour differs from the header, and plain text has none.
    HeaderPill pill;
    QVERIFY(!pill.fillColour(pill.palette()).isValid());
    pill.setFill(HeaderPill::Fill::Module, QStringLiteral("jira"));
    QVERIFY(pill.fillColour(pill.palette()) != pill.palette().color(QPalette::Window));
}

QTEST_MAIN(TestHeaderPill)
#include "tst_header_pill.moc"
