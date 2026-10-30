// Tests the built-in module "dash" and its type dash.panel: how it is
// registered, its field, and the views it makes without a page. The page
// itself is tested in tests/web/tst_dash_scheme.cpp. See the M9 plan, part 2.
#include "items/DashPanel.h"
#include "items/ItemContextImpl.h"
#include "items/ItemTypeRegistry.h"

#include <mirante/ItemContext.h>

#include <QLabel>
#include <QTest>

#include <memory>

using namespace mirante;

namespace {

// A context that is not the core's, as a module's tests would make.
class OtherContext : public ItemContext
{
public:
    QJsonObject configValue;

    QString itemName() const override { return QStringLiteral("Panel"); }
    QJsonObject config() const override { return configValue; }
    QJsonValue projectSetting(const QString &) const override { return {}; }
    QJsonObject moduleSettings(const QString &) const override { return {}; }
    QString folderPath(const QString &) const override { return {}; }
    QWidget *createWebView(const QUrl &, QWidget *) override { return nullptr; }
    QWebEngineProfile *profile() override { return nullptr; }
    QWidget *createMessageView(const QString &text, QWidget *parent) override { return new QLabel(text, parent); }
    void updateConfig(const QJsonObject &) override {}
};

void registerPlain(ModuleRegistrar &) {}

} // namespace

class TestDashPanel : public QObject
{
    Q_OBJECT

private slots:
    void registersBuiltInModule()
    {
        ItemTypeRegistry types;
        dash::registerModule(types);
        types.registerModule(QStringLiteral("plain"), &registerPlain);

        QVERIFY(types.isBuiltIn(QStringLiteral("dash")));
        QVERIFY(!types.isBuiltIn(QStringLiteral("plain")));
        QVERIFY(!types.isBuiltIn(QStringLiteral("missing")));
        QVERIFY(types.contains(dash::panelTypeId()));
        QCOMPARE(types.moduleName(QStringLiteral("dash")), QStringLiteral("Built in"));
        QVERIFY(!types.moduleUuid(QStringLiteral("dash")).isNull());
    }

    void typeIsAWebPageWithoutProfile()
    {
        ItemTypeRegistry types;
        dash::registerModule(types);
        const ItemType &type = types.type(dash::panelTypeId());
        QCOMPARE(type.rendering(), Rendering::Web);
        QVERIFY(!type.hasStatus());
        // The app profile serves it, so the item dialog offers no profile.
        QVERIFY(!types.usesProfile(dash::panelTypeId()));
    }

    void fieldChoosesThePanel()
    {
        ItemTypeRegistry types;
        dash::registerModule(types);
        const std::vector<Field> fields = types.type(dash::panelTypeId()).fields();
        QCOMPARE(fields.size(), size_t(1));
        const Field &panel = fields.front();
        QCOMPARE(panel.key, QStringLiteral("panel"));
        QCOMPARE(panel.type, Field::Type::Choice);
        QVERIFY(panel.required);
        QCOMPARE(panel.choices, dash::panelIds());
        QCOMPARE(panel.choiceLabels.size(), panel.choices.size());
        QCOMPARE(panel.defaultValue.toString(), QStringLiteral("task-summary"));
    }

    void unknownPanelShowsAMessage()
    {
        ItemTypeRegistry types;
        dash::registerModule(types);
        OtherContext context;
        context.configValue = {{QStringLiteral("panel"), QStringLiteral("nope")}};
        std::unique_ptr<QWidget> view(types.type(dash::panelTypeId()).createView(context, nullptr));
        auto *label = qobject_cast<QLabel *>(view.get());
        QVERIFY(label);
        QVERIFY(label->text().contains(QStringLiteral("Choose the panel")));
    }

    // Only the core's context makes panels.
    void otherContextShowsAMessage()
    {
        ItemTypeRegistry types;
        dash::registerModule(types);
        OtherContext context;
        context.configValue = {{QStringLiteral("panel"), QStringLiteral("group-overview")}};
        std::unique_ptr<QWidget> view(types.type(dash::panelTypeId()).createView(context, nullptr));
        auto *label = qobject_cast<QLabel *>(view.get());
        QVERIFY(label);
        QVERIFY(label->text().contains(QStringLiteral("not available")));
    }

    // Without a profile manager, as in most tests, the core's context gives a
    // message too.
    void coreContextWithoutProfiles()
    {
        PlacementServices services;
        ItemContextImpl context(services, QStringLiteral("t1"), QStringLiteral("i1"));
        std::unique_ptr<QWidget> view(context.createDashPanel(QStringLiteral("task-summary"), nullptr));
        QVERIFY(qobject_cast<QLabel *>(view.get()));
    }
};

QTEST_MAIN(TestDashPanel)
#include "tst_dash_panel.moc"
