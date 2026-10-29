// Tests ItemTypeRegistry, UnknownItemType and the id rules.
#include "items/ItemTypeIds.h"
#include "items/ItemTypeRegistry.h"
#include "items/UnknownItemType.h"

#include <QTest>

namespace {

class FakeType : public mirante::ItemType
{
public:
    FakeType(const QString &id, const QString &name)
        : m_id(id)
        , m_name(name)
    {
    }

    QString id() const override { return m_id; }
    QString name() const override { return m_name; }
    QIcon icon() const override { return {}; }
    QStringList connectionKinds() const override { return m_kinds; }

    QStringList m_kinds;

private:
    QString m_id;
    QString m_name;
};

std::unique_ptr<FakeType> withKinds(const QString &id, const QStringList &kinds)
{
    auto type = std::make_unique<FakeType>(id, id);
    type->m_kinds = kinds;
    return type;
}

void registerKinds(mirante::ModuleRegistrar &registrar)
{
    registrar.addItemType(withKinds(QStringLiteral("kinds.jira"), {QStringLiteral("jira.dc"), QStringLiteral("jira.cloud")}));
    registrar.addItemType(withKinds(QStringLiteral("kinds.mr"), {QStringLiteral("gitlab")}));
    registrar.addItemType(withKinds(QStringLiteral("kinds.issue"), {QStringLiteral("jira.cloud")}));
    registrar.addItemType(withKinds(QStringLiteral("kinds.none"), {}));
}

void registerAlpha(mirante::ModuleRegistrar &registrar)
{
    registrar.addItemType(std::make_unique<FakeType>(QStringLiteral("alpha.zeta"), QStringLiteral("Zeta")));
    registrar.addItemType(std::make_unique<FakeType>(QStringLiteral("alpha"), QStringLiteral("alpha")));
    registrar.addItemType(std::make_unique<FakeType>(QStringLiteral("alpha.mid.part"), QStringLiteral("Middle")));
}

void registerBeta(mirante::ModuleRegistrar &registrar)
{
    registrar.addItemType(std::make_unique<FakeType>(QStringLiteral("beta.one"), QStringLiteral("Beta")));
}

} // namespace

class TestRegistry : public QObject
{
    Q_OBJECT

private slots:
    void moduleIds_data()
    {
        QTest::addColumn<QString>("id");
        QTest::addColumn<bool>("valid");
        QTest::newRow("letters") << QStringLiteral("gitlab") << true;
        QTest::newRow("digits") << QStringLiteral("m2") << true;
        QTest::newRow("empty") << QString() << false;
        QTest::newRow("upper") << QStringLiteral("GitLab") << false;
        QTest::newRow("dot") << QStringLiteral("git.lab") << false;
        QTest::newRow("dash") << QStringLiteral("git-lab") << false;
        QTest::newRow("non-ascii") << QStringLiteral("café") << false;
    }

    void moduleIds()
    {
        QFETCH(QString, id);
        QFETCH(bool, valid);
        QCOMPARE(mirante::isValidModuleId(id), valid);
    }

    void typeIds_data()
    {
        QTest::addColumn<QString>("id");
        QTest::addColumn<bool>("belongs");
        QTest::newRow("module id") << QStringLiteral("gitlab") << true;
        QTest::newRow("one part") << QStringLiteral("gitlab.pipeline") << true;
        QTest::newRow("two parts") << QStringLiteral("gitlab.mr.summary") << true;
        QTest::newRow("other module") << QStringLiteral("jira.list") << false;
        QTest::newRow("longer prefix") << QStringLiteral("gitlabx.mr") << false;
        QTest::newRow("trailing dot") << QStringLiteral("gitlab.") << false;
        QTest::newRow("empty part") << QStringLiteral("gitlab..mr") << false;
        QTest::newRow("empty") << QString() << false;
    }

    void typeIds()
    {
        QFETCH(QString, id);
        QFETCH(bool, belongs);
        QCOMPARE(mirante::idBelongsToModule(id, QStringLiteral("gitlab")), belongs);
    }

    void registersTypes()
    {
        mirante::ItemTypeRegistry registry;
        registry.registerModule(QStringLiteral("alpha"), &registerAlpha);
        registry.registerModule(QStringLiteral("beta"), &registerBeta);

        QVERIFY(registry.contains(QStringLiteral("alpha")));
        QVERIFY(registry.contains(QStringLiteral("alpha.zeta")));
        QVERIFY(registry.contains(QStringLiteral("beta.one")));
        QVERIFY(!registry.contains(QStringLiteral("beta")));
        QVERIFY(!registry.contains(QStringLiteral("gamma.one")));

        const mirante::ItemType &type = registry.type(QStringLiteral("alpha.mid.part"));
        QCOMPARE(type.id(), QStringLiteral("alpha.mid.part"));
        QCOMPARE(type.name(), QStringLiteral("Middle"));
        QVERIFY(dynamic_cast<const mirante::UnknownItemType *>(&type) == nullptr);
    }

    void typesSortedByName()
    {
        mirante::ItemTypeRegistry registry;
        registry.registerModule(QStringLiteral("alpha"), &registerAlpha);
        registry.registerModule(QStringLiteral("beta"), &registerBeta);

        QStringList names;
        for (const mirante::ItemType *type : registry.types())
            names << type->name();
        // Case does not change the order.
        const QStringList expected{QStringLiteral("alpha"), QStringLiteral("Beta"), QStringLiteral("Middle"),
                                   QStringLiteral("Zeta")};
        QCOMPARE(names, expected);
    }

    void connectionKinds()
    {
        mirante::ItemTypeRegistry registry;
        registry.registerModule(QStringLiteral("alpha"), &registerAlpha);
        registry.registerModule(QStringLiteral("kinds"), &registerKinds);
        // Each kind once, sorted, whatever order the types list them in.
        const QStringList expected{QStringLiteral("gitlab"), QStringLiteral("jira.cloud"), QStringLiteral("jira.dc")};
        QCOMPARE(registry.connectionKinds(), expected);
    }

    void emptyRegistry()
    {
        const mirante::ItemTypeRegistry registry;
        QVERIFY(registry.types().empty());
        QVERIFY(registry.connectionKinds().isEmpty());
        QVERIFY(!registry.contains(QStringLiteral("web.page")));
    }

    void unknownType()
    {
        mirante::ItemTypeRegistry registry;
        registry.registerModule(QStringLiteral("alpha"), &registerAlpha);

        const mirante::ItemType &type = registry.type(QStringLiteral("timetrack.entry.list"));
        const auto *unknown = dynamic_cast<const mirante::UnknownItemType *>(&type);
        QVERIFY(unknown != nullptr);
        QCOMPARE(unknown->id(), QStringLiteral("timetrack.entry.list"));
        QCOMPARE(unknown->moduleId(), QStringLiteral("timetrack"));
        QVERIFY(unknown->name().contains(QStringLiteral("timetrack.entry.list")));
        QVERIFY(!unknown->icon().isNull());
        QVERIFY(!unknown->icon().pixmap(16, 16).isNull());

        // The same id gives the same object, so references stay valid.
        QCOMPARE(&registry.type(QStringLiteral("timetrack.entry.list")), &type);
        QVERIFY(!registry.contains(QStringLiteral("timetrack.entry.list")));
        QVERIFY(registry.types().size() == 3);
    }

    void unknownModuleIdWithoutDot()
    {
        const mirante::UnknownItemType type(QStringLiteral("notes"));
        QCOMPARE(type.moduleId(), QStringLiteral("notes"));
    }
};

QTEST_MAIN(TestRegistry)
#include "tst_registry.moc"
