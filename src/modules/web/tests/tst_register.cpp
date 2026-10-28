// Registers the web module into a fresh registry and checks its types.
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
};

QTEST_MAIN(TestRegister)
#include "tst_register.moc"
