// Tests asking the item types about a pasted address: which text is a web
// address, the order of the types that take it, the base URLs each type is
// given, and the connection that a match's base URL names.
#include "core/Entities.h"
#include "items/ItemTypeRegistry.h"
#include "items/UrlMatches.h"

#include <mirante/ItemType.h>
#include <mirante/Module.h>

#include <QTest>

#include <memory>

using namespace mirante;

namespace {

// The base URLs the server type was last given.
QList<QUrl> lastBaseUrls;

// Takes any address below one of its base URLs, naming that base.
class ServerType : public ItemType
{
public:
    QString id() const override { return QStringLiteral("testmod.server"); }
    QString name() const override { return QStringLiteral("Zeta server"); }
    QIcon icon() const override { return {}; }
    QStringList connectionKinds() const override { return {QStringLiteral("srv.a"), QStringLiteral("srv.b")}; }

    std::optional<UrlMatch> fromUrl(const QUrl &url, const QList<QUrl> &baseUrls) const override
    {
        lastBaseUrls = baseUrls;
        for (const QUrl &base : baseUrls) {
            if (pathBelow(url, base)) {
                UrlMatch match;
                match.config.insert(QStringLiteral("path"), *pathBelow(url, base));
                match.baseUrl = base;
                return match;
            }
        }
        return std::nullopt;
    }
};

// Takes every address, as a generic match. Its name sorts first.
class AnyType : public ItemType
{
public:
    QString id() const override { return QStringLiteral("testmod.any"); }
    QString name() const override { return QStringLiteral("Alpha any"); }
    QIcon icon() const override { return {}; }

    std::optional<UrlMatch> fromUrl(const QUrl &, const QList<QUrl> &) const override
    {
        UrlMatch match;
        match.generic = true;
        return match;
    }
};

// Takes nothing, as the default.
class NoneType : public ItemType
{
public:
    QString id() const override { return QStringLiteral("testmod.none"); }
    QString name() const override { return QStringLiteral("Beta none"); }
    QIcon icon() const override { return {}; }
};

void registerTestModule(ModuleRegistrar &registrar)
{
    registrar.addItemType(std::make_unique<ServerType>());
    registrar.addItemType(std::make_unique<AnyType>());
    registrar.addItemType(std::make_unique<NoneType>());
}

Connection connection(const QString &id, const QString &kind, const QString &baseUrl)
{
    Connection result;
    result.id = id;
    result.kind = kind;
    result.baseUrl = baseUrl;
    return result;
}

} // namespace

class TestUrlMatches : public QObject
{
    Q_OBJECT

    ItemTypeRegistry types;

private slots:
    void initTestCase() { types.registerModule(QStringLiteral("testmod"), registerTestModule); }

    void webAddress()
    {
        QCOMPARE(urlmatches::webAddress(QStringLiteral("  https://example.com/a?b=1 \n")),
                 QUrl(QStringLiteral("https://example.com/a?b=1")));
        QCOMPARE(urlmatches::webAddress(QStringLiteral("http://intranet")), QUrl(QStringLiteral("http://intranet")));
        QVERIFY(urlmatches::webAddress(QString()).isEmpty());
        QVERIFY(urlmatches::webAddress(QStringLiteral("example.com")).isEmpty());
        QVERIFY(urlmatches::webAddress(QStringLiteral("ftp://example.com")).isEmpty());
        QVERIFY(urlmatches::webAddress(QStringLiteral("mailto:a@example.com")).isEmpty());
        QVERIFY(urlmatches::webAddress(QStringLiteral("https://a.com and more")).isEmpty());
        QVERIFY(urlmatches::webAddress(QStringLiteral("two\nhttps://lines.com")).isEmpty());
    }

    // Types of modules come first, then generic ones; the rest are left out.
    void order()
    {
        const std::vector<Connection> connections{connection(QStringLiteral("c1"), QStringLiteral("srv.a"),
                                                             QStringLiteral("https://srv.example"))};
        const auto matches =
            urlmatches::match(types, QUrl(QStringLiteral("https://srv.example/x/y")), connections);
        QCOMPARE(matches.size(), size_t(2));
        QCOMPARE(matches.at(0).type->id(), QStringLiteral("testmod.server"));
        QCOMPARE(matches.at(0).match.config.value(QStringLiteral("path")).toString(), QStringLiteral("x/y"));
        QCOMPARE(matches.at(0).connectionId, QStringLiteral("c1"));
        QCOMPARE(matches.at(1).type->id(), QStringLiteral("testmod.any"));
        QVERIFY(matches.at(1).connectionId.isEmpty());

        // An address of no known server leaves the generic type only.
        const auto other = urlmatches::match(types, QUrl(QStringLiteral("https://other.example/")), connections);
        QCOMPARE(other.size(), size_t(1));
        QCOMPARE(other.at(0).type->id(), QStringLiteral("testmod.any"));

        // No address, no matches.
        QVERIFY(urlmatches::match(types, QUrl(), connections).empty());
    }

    // A type gets the base URLs of its own kinds only, in its order of kinds,
    // and the first connection with the matched base URL is named.
    void baseUrlsAndConnection()
    {
        const std::vector<Connection> connections{
            connection(QStringLiteral("b1"), QStringLiteral("srv.b"), QStringLiteral("https://b.example/")),
            connection(QStringLiteral("x1"), QStringLiteral("other"), QStringLiteral("https://x.example")),
            connection(QStringLiteral("a1"), QStringLiteral("srv.a"), QStringLiteral("https://a.example/sub")),
            connection(QStringLiteral("b2"), QStringLiteral("srv.b"), QStringLiteral("https://B.example")),
        };
        const auto matches = urlmatches::match(types, QUrl(QStringLiteral("https://b.example/p")), connections);
        QCOMPARE(lastBaseUrls, (QList<QUrl>{QUrl(QStringLiteral("https://a.example/sub")),
                                            QUrl(QStringLiteral("https://b.example/")),
                                            QUrl(QStringLiteral("https://B.example"))}));
        QCOMPARE(matches.at(0).connectionId, QStringLiteral("b1"));
    }
};

QTEST_MAIN(TestUrlMatches)
#include "tst_url_matches.moc"
