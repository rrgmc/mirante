// Registers the web module into a fresh registry and checks its types.
#include "items/FieldValidation.h"
#include "items/ItemTypeRegistry.h"

#include <QIcon>
#include <QPixmap>
#include <QTest>

namespace mirante::modules::web {
void registerModule(mirante::ModuleRegistrar &registrar);
}

class TestRegister : public QObject
{
    Q_OBJECT

private slots:
    void registersTypes()
    {
        mirante::ItemTypeRegistry registry;
        registry.registerModule(QStringLiteral("web"), &mirante::modules::web::registerModule);

        QStringList ids;
        for (const mirante::ItemType *type : registry.types())
            ids << type->id();
        QCOMPARE(ids, QStringList{QStringLiteral("web.page")});
        QVERIFY(registry.contains(QStringLiteral("web.page")));
        QCOMPARE(registry.type(QStringLiteral("web.page")).name(), QStringLiteral("Web page"));
    }

    void iconsRender()
    {
        mirante::ItemTypeRegistry registry;
        registry.registerModule(QStringLiteral("web"), &mirante::modules::web::registerModule);

        for (const mirante::ItemType *type : registry.types()) {
            const QIcon icon = type->icon();
            QVERIFY2(!icon.isNull(), qPrintable(type->id()));
            QVERIFY2(!icon.pixmap(16, 16).isNull(), qPrintable(type->id()));
        }
    }
    void fieldKeys()
    {
        mirante::ItemTypeRegistry registry;
        registry.registerModule(QStringLiteral("web"), &mirante::modules::web::registerModule);

        QMap<QString, QStringList> expected;
        expected.insert(QStringLiteral("web.page"), {QStringLiteral("url")});

        for (auto it = expected.cbegin(); it != expected.cend(); ++it) {
            const mirante::ItemType &type = registry.type(it.key());
            QStringList keys;
            for (const mirante::Field &field : type.fields())
                keys << field.key;
            QCOMPARE(keys, it.value());
            QVERIFY(type.pastedImageField().isEmpty());
        }
    }

    // Keys and labels are set and keys are unique. A new item's defaults
    // validate, except that each required field without a default is reported.
    void fieldsValidateTheirDefaults()
    {
        mirante::ItemTypeRegistry registry;
        registry.registerModule(QStringLiteral("web"), &mirante::modules::web::registerModule);

        for (const mirante::ItemType *type : registry.types()) {
            const std::vector<mirante::Field> fields = type->fields();
            QStringList keys;
            QStringList missing;
            for (const mirante::Field &field : fields) {
                const QString where = type->id() + QLatin1Char(' ') + field.key;
                QVERIFY2(!field.key.isEmpty(), qPrintable(type->id()));
                QVERIFY2(!keys.contains(field.key), qPrintable(where));
                QVERIFY2(!field.label.isEmpty(), qPrintable(where));
                if (field.type == mirante::Field::Type::Choice) {
                    QVERIFY2(!field.choices.isEmpty(), qPrintable(where));
                    QVERIFY2(field.choiceLabels.size() == field.choices.size(), qPrintable(where));
                }
                keys << field.key;
                if (field.required && (field.defaultValue.isNull() || field.defaultValue.isUndefined()))
                    missing << field.key;
            }

            QStringList reported;
            for (const mirante::FieldProblem &problem : mirante::validate(fields, mirante::defaultConfig(fields)))
                reported << problem.key;
            QCOMPARE(reported, missing);
        }
    }

    // The module has a name and a description, and so has every type, for
    // Help > Modules. See docs/decisions/0056-modules-window.md.
    void namesAndDescriptions()
    {
        mirante::ItemTypeRegistry registry;
        registry.registerModule(QStringLiteral("web"), &mirante::modules::web::registerModule);

        QCOMPARE(registry.moduleName(QStringLiteral("web")), QStringLiteral("Web"));
        QVERIFY(!registry.moduleDescription(QStringLiteral("web")).isEmpty());
        for (const mirante::ItemType *type : registry.types())
            QVERIFY2(!type->description().isEmpty(), qPrintable(type->id()));
    }
};

QTEST_MAIN(TestRegister)
#include "tst_register.moc"
