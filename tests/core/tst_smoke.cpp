// Checks that the test helpers work: the test builds, links mirante_core and runs.
#include "core/Entities.h"

#include <QTest>

class TestSmoke : public QObject
{
    Q_OBJECT

private slots:
    void entityDefaults()
    {
        const mirante::Task task;
        QCOMPARE(task.priority, mirante::Priority::Normal);
        QCOMPARE(task.state, mirante::TaskState::Active);
    }
};

QTEST_GUILESS_MAIN(TestSmoke)
#include "tst_smoke.moc"
