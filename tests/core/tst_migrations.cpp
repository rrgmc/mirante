// Tests the schemaVersion migrations of data folder files.
#include "core/Migrations.h"

#include <QTest>

using namespace mirante::migrations;

namespace {

// Two fake steps. Each appends its number to "trail", so the order shows.
std::vector<Step> fakeSteps()
{
    auto step = [](int number) {
        return [number](FileKind, QJsonObject &json) {
            json.insert("trail", json.value("trail").toString() + QString::number(number));
        };
    };
    return {step(1), step(2)};
}

QJsonObject file(const QJsonValue &version)
{
    QJsonObject json{{"name", "Task"}};
    if (!version.isUndefined())
        json.insert("schemaVersion", version);
    return json;
}

} // namespace

class TestMigrations : public QObject
{
    Q_OBJECT

private slots:
    void currentVersionIsUnchanged()
    {
        QJsonObject json = file(currentSchemaVersion);
        const QJsonObject before = json;
        QString error;
        QVERIFY(migrate(FileKind::Task, json, &error));
        QCOMPARE(json, before);
        QVERIFY(error.isEmpty());
    }

    void olderVersionRunsStepsInOrder()
    {
        QJsonObject json = file(1);
        QVERIFY(migrate(FileKind::Group, json, fakeSteps()));
        QCOMPARE(json.value("trail").toString(), QString("12"));
        QCOMPARE(json.value("schemaVersion").toInt(), 3);
        QCOMPARE(json.value("name").toString(), QString("Task"));
    }

    void stepsStartAtTheFileVersion()
    {
        QJsonObject json = file(2);
        QVERIFY(migrate(FileKind::Project, json, fakeSteps()));
        QCOMPARE(json.value("trail").toString(), QString("2"));
        QCOMPARE(json.value("schemaVersion").toInt(), 3);
    }

    void stepsGetTheKind()
    {
        FileKind seen = FileKind::Task;
        const std::vector<Step> steps{[&seen](FileKind kind, QJsonObject &) { seen = kind; }};
        QJsonObject json = file(1);
        QVERIFY(migrate(FileKind::RootDashboard, json, steps));
        QCOMPARE(seen, FileKind::RootDashboard);
    }

    void newerVersionFails_data()
    {
        QTest::addColumn<QJsonValue>("version");
        QTest::addColumn<QString>("error");
        QTest::newRow("newer") << QJsonValue(4) << QString("written by a newer version of Mirante");
        QTest::newRow("missing") << QJsonValue(QJsonValue::Undefined) << QString("missing schemaVersion");
        QTest::newRow("string") << QJsonValue("1") << QString("invalid schemaVersion");
        QTest::newRow("null") << QJsonValue(QJsonValue::Null) << QString("invalid schemaVersion");
        QTest::newRow("zero") << QJsonValue(0) << QString("invalid schemaVersion");
        QTest::newRow("fraction") << QJsonValue(1.5) << QString("invalid schemaVersion");
    }

    void newerVersionFails()
    {
        QFETCH(QJsonValue, version);
        QFETCH(QString, error);
        QJsonObject json = file(version);
        const QJsonObject before = json;
        QString message;
        QVERIFY(!migrate(FileKind::Task, json, fakeSteps(), &message));
        QCOMPARE(message, error);
        QCOMPARE(json, before);
    }

    void errorIsOptional()
    {
        QJsonObject json = file(QJsonValue::Undefined);
        QVERIFY(!migrate(FileKind::Workspace, json));
    }

    void realStepsReachTheCurrentVersion()
    {
        QJsonObject json = file(1);
        QVERIFY(migrate(FileKind::Workspace, json));
        QCOMPARE(json.value("schemaVersion").toInt(), currentSchemaVersion);
        QJsonObject newer = file(currentSchemaVersion + 1);
        QVERIFY(!migrate(FileKind::Workspace, newer));
    }
};

QTEST_GUILESS_MAIN(TestMigrations)
#include "tst_migrations.moc"
