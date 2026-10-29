// Reading a pasted address as a Web page: any http or https address, named
// after its host, as a generic match.
#include "../WebUrls.h"

#include "items/ItemTypeRegistry.h"

#include <mirante/ItemType.h>

#include <QJsonObject>
#include <QTest>

namespace mirante::modules::web {
void registerModule(mirante::ModuleRegistrar &registrar);
}

using mirante::modules::web::matchUrl;

class TestFromUrl : public QObject
{
    Q_OBJECT

private slots:
    void anyWebAddress()
    {
        const auto match = matchUrl(QUrl(QStringLiteral("https://www.example.com/docs/page?x=1#top")));
        QVERIFY(match);
        QCOMPARE(match->config, (QJsonObject{{QStringLiteral("url"),
                                              QStringLiteral("https://www.example.com/docs/page?x=1#top")}}));
        QCOMPARE(match->name, QStringLiteral("example.com"));
        QVERIFY(match->generic);
        QVERIFY(match->baseUrl.isEmpty());

        const auto plain = matchUrl(QUrl(QStringLiteral("http://intranet:8080/")));
        QVERIFY(plain);
        QCOMPARE(plain->name, QStringLiteral("intranet"));
    }

    void refused()
    {
        QVERIFY(!matchUrl(QUrl()));
        QVERIFY(!matchUrl(QUrl(QStringLiteral("ftp://example.com/file"))));
        QVERIFY(!matchUrl(QUrl(QStringLiteral("file:///tmp/a.html"))));
        QVERIFY(!matchUrl(QUrl(QStringLiteral("mailto:someone@example.com"))));
        QVERIFY(!matchUrl(QUrl(QStringLiteral("https:///nohost"))));
    }

    void registeredType()
    {
        mirante::ItemTypeRegistry registry;
        registry.registerModule(QStringLiteral("web"), &mirante::modules::web::registerModule);
        const auto match = registry.type(QStringLiteral("web.page")).fromUrl(QUrl(QStringLiteral("https://a.b/")), {});
        QVERIFY(match);
        QVERIFY(match->generic);
    }
};

QTEST_MAIN(TestFromUrl)
#include "tst_from_url.moc"
