// Tests compactDate: this year, another year, local time and an invalid date.
#include <mirante/widgets/CompactDate.h>

#include <QTest>
#include <QTimeZone>

using namespace mirante::widgets;

class TestCompactDate : public QObject
{
    Q_OBJECT

private slots:
    void thisYearShowsMonthDayAndTime()
    {
        const QDateTime when(QDate(QDate::currentDate().year(), 3, 7), QTime(14, 32), QTimeZone::LocalTime);
        QCOMPARE(compactDate(when), QStringLiteral("03-07 14:32"));
    }

    void anotherYearShowsTheFullDate()
    {
        const int year = QDate::currentDate().year() - 1;
        const QDateTime when(QDate(year, 10, 30), QTime(14, 32), QTimeZone::LocalTime);
        QCOMPARE(compactDate(when), QStringLiteral("%1-10-30").arg(year));
    }

    void usesLocalTime()
    {
        const int year = QDate::currentDate().year();
        const QDateTime utc(QDate(year, 6, 15), QTime(12, 0), QTimeZone::UTC);
        const QDateTime local = utc.toLocalTime();
        QCOMPARE(compactDate(utc), local.toString(QStringLiteral("MM-dd HH:mm")));
    }

    void invalidIsEmpty() { QVERIFY(compactDate(QDateTime()).isEmpty()); }
};

QTEST_GUILESS_MAIN(TestCompactDate)
#include "tst_compact_date.moc"
