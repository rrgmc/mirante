// Tests ViewPool: the limit, the order of unloading, that a visible placement
// is never unloaded, that a removed placement is forgotten, and callbacks that
// call back into the pool.
#include "items/ViewPool.h"

#include <QStringList>
#include <QTest>

#include <memory>
#include <vector>

using namespace mirante;

class TestViewPool : public QObject
{
    Q_OBJECT

    // The placements of one test, named p0, p1 and so on, and the names of
    // those unloaded, in order.
    std::vector<std::unique_ptr<QObject>> placements;
    QStringList unloaded;

    QObject *make(ViewPool &pool)
    {
        auto placement = std::make_unique<QObject>();
        placement->setObjectName(QStringLiteral("p%1").arg(placements.size()));
        QObject *raw = placement.get();
        placements.push_back(std::move(placement));
        pool.add(raw, [this, raw] { unloaded << raw->objectName(); });
        return raw;
    }

private slots:
    void init()
    {
        unloaded.clear();
    }

    void cleanup()
    {
        placements.clear();
    }

    void defaultLimitIsEight()
    {
        ViewPool pool;
        QCOMPARE(pool.limit(), 8);
        for (int i = 0; i < 9; ++i)
            pool.setVisible(make(pool), false);
        QCOMPARE(unloaded, QStringList{QStringLiteral("p0")});
        QCOMPARE(pool.hiddenCount(), 8);
    }

    void hiddenUpToTheLimitStayLoaded()
    {
        ViewPool pool;
        pool.setLimit(2);
        QObject *a = make(pool);
        QObject *b = make(pool);
        QCOMPARE(pool.hiddenCount(), 0);
        pool.setVisible(a, false);
        pool.setVisible(b, false);
        QCOMPARE(pool.hiddenCount(), 2);
        QVERIFY(unloaded.isEmpty());
    }

    // Those hidden longest go first, whatever order they were added in.
    void unloadsHiddenLongestFirst()
    {
        ViewPool pool;
        pool.setLimit(1);
        QObject *a = make(pool);
        QObject *b = make(pool);
        QObject *c = make(pool);
        pool.setVisible(c, false);
        pool.setVisible(a, false);
        QCOMPARE(unloaded, QStringList{QStringLiteral("p2")});
        pool.setVisible(b, false);
        QCOMPARE(unloaded, (QStringList{QStringLiteral("p2"), QStringLiteral("p0")}));
        QCOMPARE(pool.hiddenCount(), 1);
    }

    // Showing a placement again and hiding it later makes it the newest hidden.
    void showingAgainResetsTheOrder()
    {
        ViewPool pool;
        pool.setLimit(2);
        QObject *a = make(pool);
        QObject *b = make(pool);
        QObject *c = make(pool);
        pool.setVisible(a, false);
        pool.setVisible(b, false);
        pool.setVisible(a, true);
        pool.setVisible(a, false);
        pool.setVisible(c, false);
        QCOMPARE(unloaded, QStringList{QStringLiteral("p1")});
    }

    // Hiding a placement that is already hidden keeps its place in the order.
    void hidingTwiceKeepsTheOrder()
    {
        ViewPool pool;
        pool.setLimit(1);
        QObject *a = make(pool);
        QObject *b = make(pool);
        pool.setVisible(a, false);
        pool.setVisible(b, false);
        QCOMPARE(unloaded, QStringList{QStringLiteral("p0")});
        QObject *c = make(pool);
        pool.setVisible(b, false);
        pool.setVisible(c, false);
        QCOMPARE(unloaded, (QStringList{QStringLiteral("p0"), QStringLiteral("p1")}));
    }

    void visibleIsNeverUnloaded()
    {
        ViewPool pool;
        pool.setLimit(0);
        QObject *a = make(pool);
        make(pool);
        make(pool);
        QVERIFY(unloaded.isEmpty());
        QCOMPARE(pool.hiddenCount(), 0);
        pool.setVisible(a, false);
        QCOMPARE(unloaded, QStringList{QStringLiteral("p0")});
    }

    // An unloaded placement is forgotten. It adds itself again when it loads.
    void unloadedIsForgotten()
    {
        ViewPool pool;
        pool.setLimit(0);
        QObject *a = make(pool);
        pool.setVisible(a, false);
        QCOMPARE(unloaded.size(), 1);
        pool.setVisible(a, true);
        pool.setVisible(a, false);
        QCOMPARE(unloaded.size(), 1);
        pool.add(a, [this] { unloaded << QStringLiteral("again"); });
        pool.setVisible(a, false);
        QCOMPARE(unloaded, (QStringList{QStringLiteral("p0"), QStringLiteral("again")}));
    }

    void removedIsForgotten()
    {
        ViewPool pool;
        pool.setLimit(1);
        QObject *a = make(pool);
        QObject *b = make(pool);
        pool.setVisible(a, false);
        QCOMPARE(pool.hiddenCount(), 1);
        pool.remove(a);
        QCOMPARE(pool.hiddenCount(), 0);
        pool.setVisible(b, false);
        pool.setVisible(a, false);
        QVERIFY(unloaded.isEmpty());
        // Removing what the pool does not know does nothing.
        pool.remove(a);
        QCOMPARE(pool.hiddenCount(), 1);
    }

    // A placement deleted without remove is forgotten too.
    void deletedIsForgotten()
    {
        ViewPool pool;
        pool.setLimit(1);
        QObject *a = make(pool);
        QObject *b = make(pool);
        pool.setVisible(a, false);
        placements.front().reset();
        QCOMPARE(pool.hiddenCount(), 0);
        pool.setVisible(b, false);
        QVERIFY(unloaded.isEmpty());
    }

    void loweringTheLimitUnloadsAtOnce()
    {
        ViewPool pool;
        pool.setLimit(4);
        for (int i = 0; i < 4; ++i)
            pool.setVisible(make(pool), false);
        QVERIFY(unloaded.isEmpty());
        pool.setLimit(1);
        QCOMPARE(unloaded, (QStringList{QStringLiteral("p0"), QStringLiteral("p1"), QStringLiteral("p2")}));
        QCOMPARE(pool.hiddenCount(), 1);
        // Raising it unloads nothing.
        pool.setLimit(8);
        QCOMPARE(unloaded.size(), 3);
        // A negative limit counts as none.
        pool.setLimit(-1);
        QCOMPARE(pool.limit(), 0);
        QCOMPARE(pool.hiddenCount(), 0);
        QCOMPARE(unloaded.size(), 4);
    }

    // An unload callback that removes another hidden placement and hides one
    // more, which asks the pool to unload again while it is unloading.
    void callbacksMayCallBack()
    {
        ViewPool pool;
        pool.setLimit(4);
        QObject *a = make(pool);
        QObject *b = make(pool);
        QObject *c = make(pool);
        QObject *e = make(pool);
        QObject *f = make(pool);
        pool.add(a, [&] {
            unloaded << QStringLiteral("p0");
            pool.remove(a);
            pool.remove(b);
            pool.setVisible(f, false);
        });
        pool.setVisible(a, false);
        pool.setVisible(b, false);
        pool.setVisible(c, false);
        pool.setVisible(e, false);
        QVERIFY(unloaded.isEmpty());
        // Lowering the limit unloads a. Its callback removes b and hides f,
        // which leaves c, e and f hidden. One more must go, and it is c.
        pool.setLimit(2);
        QCOMPARE(unloaded, (QStringList{QStringLiteral("p0"), QStringLiteral("p2")}));
        QCOMPARE(pool.hiddenCount(), 2);
    }

    // An unload callback that adds the placement again, as a placement that
    // loads again at once would.
    void callbackMayAddAgain()
    {
        ViewPool pool;
        pool.setLimit(0);
        QObject *a = make(pool);
        pool.add(a, [&] {
            unloaded << QStringLiteral("p0");
            pool.add(a, [this] { unloaded << QStringLiteral("again"); });
        });
        pool.setVisible(a, false);
        QCOMPARE(unloaded, QStringList{QStringLiteral("p0")});
        QCOMPARE(pool.hiddenCount(), 0);
        pool.setVisible(a, false);
        QCOMPARE(unloaded, (QStringList{QStringLiteral("p0"), QStringLiteral("again")}));
    }
};

QTEST_GUILESS_MAIN(TestViewPool)
#include "tst_view_pool.moc"
