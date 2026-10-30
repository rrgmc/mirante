// Tests ItemTypeRegistry, UnknownItemType and the id rules.
#include "items/ItemTypeIds.h"
#include "items/ItemTypeRegistry.h"
#include "items/UnknownItemType.h"

#include <QRegularExpression>
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
    mirante::Rendering rendering() const override
    {
        return m_web ? mirante::Rendering::Web : mirante::Rendering::Native;
    }

    bool m_web = false;

private:
    QString m_id;
    QString m_name;
};

void registerWeb(mirante::ModuleRegistrar &registrar)
{
    auto page = std::make_unique<FakeType>(QStringLiteral("webby.page"), QStringLiteral("Page"));
    page->m_web = true;
    registrar.addItemType(std::move(page));
    registrar.addItemType(std::make_unique<FakeType>(QStringLiteral("webby.plain"), QStringLiteral("Plain")));
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

class FakeScreen : public mirante::SettingsScreen
{
public:
    FakeScreen(const QString &id, const QString &name)
        : m_id(id)
        , m_name(name)
    {
    }

    QString id() const override { return m_id; }
    QString name() const override { return m_name; }

private:
    QString m_id;
    QString m_name;
};

// What the last registerScreens() call got from registrar.settings().
const mirante::ModuleSettings *registeredSettings = nullptr;

void registerScreens(mirante::ModuleRegistrar &registrar)
{
    registrar.setModuleName(QStringLiteral("Alpha Tools"));
    registrar.addSettingsScreen(std::make_unique<FakeScreen>(QStringLiteral("general"), QStringLiteral("General")));
    registrar.addSettingsScreen(std::make_unique<FakeScreen>(QStringLiteral("accounts2"), QStringLiteral("Accounts")));
    registeredSettings = &registrar.settings();
}

mirante::Field field(const QString &key, mirante::Field::Type type = mirante::Field::Type::Text)
{
    mirante::Field result;
    result.key = key;
    result.label = key + QStringLiteral(" label");
    result.type = type;
    return result;
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

    void emptyRegistry()
    {
        const mirante::ItemTypeRegistry registry;
        QVERIFY(registry.types().empty());
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
        QVERIFY(!unknown->moduleInBuild());
        QVERIFY(unknown->name().contains(QStringLiteral("timetrack.entry.list")));
        QVERIFY(!unknown->icon().isNull());
        QVERIFY(!unknown->icon().pixmap(16, 16).isNull());

        // The same id gives the same object, so references stay valid.
        QCOMPARE(&registry.type(QStringLiteral("timetrack.entry.list")), &type);
        QVERIFY(!registry.contains(QStringLiteral("timetrack.entry.list")));
        QVERIFY(registry.types().size() == 3);

        // A type the module no longer has: the module is in the build.
        const auto *removed = dynamic_cast<const mirante::UnknownItemType *>(&registry.type(QStringLiteral("alpha.removed")));
        QVERIFY(removed != nullptr);
        QVERIFY(removed->moduleInBuild());
    }

    void unknownModuleIdWithoutDot()
    {
        const mirante::UnknownItemType type(QStringLiteral("notes"));
        QCOMPARE(type.moduleId(), QStringLiteral("notes"));
    }

    // Screens are kept per module in the order added. A module without a
    // name is named by its id.
    void settingsScreens()
    {
        mirante::ItemTypeRegistry registry;
        registry.registerModule(QStringLiteral("beta"), &registerBeta);
        registry.registerModule(QStringLiteral("alpha"), &registerScreens);

        QCOMPARE(registry.moduleIds(), (QStringList{QStringLiteral("alpha"), QStringLiteral("beta")}));
        QCOMPARE(registry.moduleName(QStringLiteral("alpha")), QStringLiteral("Alpha Tools"));
        QCOMPARE(registry.moduleName(QStringLiteral("beta")), QStringLiteral("beta"));
        QCOMPARE(registry.moduleName(QStringLiteral("none")), QStringLiteral("none"));

        const std::vector<const mirante::SettingsScreen *> screens =
            registry.settingsScreens(QStringLiteral("alpha"));
        QCOMPARE(screens.size(), std::size_t(2));
        QCOMPARE(screens[0]->id(), QStringLiteral("general"));
        QCOMPARE(screens[1]->id(), QStringLiteral("accounts2"));
        QCOMPARE(screens[1]->name(), QStringLiteral("Accounts"));
        QCOMPARE(registry.settingsScreen(QStringLiteral("alpha"), QStringLiteral("accounts2")), screens[1]);
        QVERIFY(!registry.settingsScreen(QStringLiteral("alpha"), QStringLiteral("none")));
        QVERIFY(registry.settingsScreens(QStringLiteral("beta")).empty());
        QVERIFY(registry.settingsScreens(QStringLiteral("none")).empty());
        QVERIFY(!registry.moduleSettings(QStringLiteral("none")));
    }

    // A module's description is kept as given, and is empty without one. A
    // type's description defaults to empty.
    void descriptions()
    {
        mirante::ItemTypeRegistry registry;
        registry.registerModule(QStringLiteral("beta"), &registerBeta);
        registry.registerModule(QStringLiteral("alpha"), [](mirante::ModuleRegistrar &registrar) {
            registrar.setModuleDescription(QStringLiteral("Tools for alpha work."));
            registerAlpha(registrar);
        });

        QCOMPARE(registry.moduleDescription(QStringLiteral("alpha")), QStringLiteral("Tools for alpha work."));
        QVERIFY(registry.moduleDescription(QStringLiteral("beta")).isEmpty());
        QVERIFY(registry.moduleDescription(QStringLiteral("none")).isEmpty());
        QVERIFY(registry.type(QStringLiteral("beta.one")).description().isEmpty());
    }

    // A module's types, sorted by name; none for an unknown module.
    void moduleTypes()
    {
        mirante::ItemTypeRegistry registry;
        registry.registerModule(QStringLiteral("alpha"), &registerAlpha);
        registry.registerModule(QStringLiteral("beta"), &registerBeta);

        QStringList ids;
        for (const mirante::ItemType *type : registry.moduleTypes(QStringLiteral("alpha")))
            ids << type->id();
        QCOMPARE(ids, (QStringList{QStringLiteral("alpha"), QStringLiteral("alpha.mid.part"),
                                   QStringLiteral("alpha.zeta")}));
        QCOMPARE(registry.moduleTypes(QStringLiteral("beta")).size(), std::size_t(1));
        QVERIFY(registry.moduleTypes(QStringLiteral("none")).empty());
    }

    // A module's ModuleSettings reads through the reader, which may be set
    // after registering, and asks for the module's own id only, in the
    // profile it is given.
    void moduleSettingsReadThroughTheReader()
    {
        mirante::ItemTypeRegistry registry;
        registry.registerModule(QStringLiteral("alpha"), &registerScreens);
        QVERIFY(registry.moduleSettings(QStringLiteral("alpha")) == registeredSettings);
        QVERIFY(registeredSettings->values(QStringLiteral("work"), QStringLiteral("general")).isEmpty());

        QStringList asked;
        registry.setSettingsReader(
            [&asked](const QString &profileId, const QString &moduleId, const QString &screenId) {
                asked << profileId + QLatin1Char('/') + moduleId + QLatin1Char('/') + screenId;
                return QJsonObject{{QStringLiteral("key"), screenId}};
            });
        QCOMPARE(registeredSettings->values(QStringLiteral("work"), QStringLiteral("general"))
                     .value(QStringLiteral("key"))
                     .toString(),
                 QStringLiteral("general"));
        QCOMPARE(asked, QStringList{QStringLiteral("work/alpha/general")});
    }

    // A type uses a profile when it makes web views or its module has
    // settings screens.
    void usesProfile()
    {
        mirante::ItemTypeRegistry registry;
        registry.registerModule(QStringLiteral("alpha"), &registerAlpha);
        registry.registerModule(QStringLiteral("webby"), &registerWeb);
        registry.registerModule(QStringLiteral("gamma"), [](mirante::ModuleRegistrar &registrar) {
            registerScreens(registrar);
            registrar.addItemType(std::make_unique<FakeType>(QStringLiteral("gamma.thing"), QStringLiteral("Thing")));
        });
        QVERIFY(!registry.usesProfile(QStringLiteral("alpha.zeta")));
        QVERIFY(registry.usesProfile(QStringLiteral("webby.page")));
        QVERIFY(!registry.usesProfile(QStringLiteral("webby.plain")));
        QVERIFY(registry.usesProfile(QStringLiteral("gamma.thing")));
        QVERIFY(!registry.usesProfile(QStringLiteral("missing.type")));
    }

    // Project settings are kept per module, in the order they were added.
    // Only modules with some are listed.
    void projectSettings()
    {
        mirante::ItemTypeRegistry registry;
        registry.registerModule(QStringLiteral("beta"), [](mirante::ModuleRegistrar &registrar) {
            registrar.addProjectSetting(field(QStringLiteral("zone")));
            registrar.addProjectSetting(field(QStringLiteral("area"), mirante::Field::Type::Choice));
        });
        registry.registerModule(QStringLiteral("alpha"), [](mirante::ModuleRegistrar &registrar) {
            registrar.addProjectSetting(field(QStringLiteral("path")));
        });
        registry.registerModule(QStringLiteral("gamma"), &registerScreens);

        const std::vector<mirante::Field> beta = registry.projectSettings(QStringLiteral("beta"));
        QCOMPARE(beta.size(), std::size_t(2));
        QCOMPARE(beta[0].key, QStringLiteral("zone"));
        QCOMPARE(beta[0].label, QStringLiteral("zone label"));
        QCOMPARE(beta[1].key, QStringLiteral("area"));
        QCOMPARE(beta[1].type, mirante::Field::Type::Choice);
        QCOMPARE(registry.projectSettings(QStringLiteral("alpha")).size(), std::size_t(1));
        QVERIFY(registry.projectSettings(QStringLiteral("gamma")).empty());
        QVERIFY(registry.projectSettings(QStringLiteral("none")).empty());
        QCOMPARE(registry.projectSettingModules(), (QStringList{QStringLiteral("alpha"), QStringLiteral("beta")}));
    }

    // Secret, Folder and Image fields, fields without a key and repeated keys
    // are left out with a warning. The other fields are kept.
    void projectSettingsRefused()
    {
        const QRegularExpression warning(QStringLiteral("project setting .* left out"));
        for (int i = 0; i < 5; ++i)
            QTest::ignoreMessage(QtWarningMsg, warning);
        mirante::ItemTypeRegistry registry;
        registry.registerModule(QStringLiteral("alpha"), [](mirante::ModuleRegistrar &registrar) {
            registrar.addProjectSetting(field(QStringLiteral("token"), mirante::Field::Type::Secret));
            registrar.addProjectSetting(field(QStringLiteral("folder"), mirante::Field::Type::Folder));
            registrar.addProjectSetting(field(QStringLiteral("image"), mirante::Field::Type::Image));
            registrar.addProjectSetting(field(QString()));
            registrar.addProjectSetting(field(QStringLiteral("path")));
            registrar.addProjectSetting(field(QStringLiteral("path"), mirante::Field::Type::Url));
        });
        const std::vector<mirante::Field> kept = registry.projectSettings(QStringLiteral("alpha"));
        QCOMPARE(kept.size(), std::size_t(1));
        QCOMPARE(kept[0].key, QStringLiteral("path"));
        QCOMPARE(kept[0].type, mirante::Field::Type::Text);
    }
};

QTEST_MAIN(TestRegistry)
#include "tst_registry.moc"
