// The address of the web page type, and the message it shows instead.
#include "../WebUrls.h"

#include "items/ItemTypeRegistry.h"

#include <mirante/ItemContext.h>
#include <mirante/ItemType.h>

#include <QJsonObject>
#include <QTest>
#include <QWidget>

#include <memory>
#include <optional>

namespace mirante::modules::web {
void registerModule(mirante::ModuleRegistrar &registrar);
}

using mirante::modules::web::pageUrl;

namespace {

// An item context with a configuration and no connection. Each view it makes
// is a plain widget with the address or the message in a property.
class FakeContext : public mirante::ItemContext
{
public:
    QJsonObject itemConfig;

    QString itemName() const override { return QStringLiteral("Item"); }
    QJsonObject config() const override { return itemConfig; }
    QJsonValue projectSetting(const QString &) const override { return {}; }
    QJsonObject moduleSettings(const QString &) const override { return {}; }
    std::optional<mirante::ConnectionDetails> connection(const QStringList &) const override { return {}; }
    QString folderPath(const QString &) const override { return {}; }

    QWidget *createWebView(const QUrl &url, QWidget *parent) override
    {
        auto *view = new QWidget(parent);
        view->setProperty("url", url);
        return view;
    }

    QWebEngineProfile *profile() override { return nullptr; }

    QWidget *createMessageView(const QString &text, QWidget *parent) override
    {
        auto *view = new QWidget(parent);
        view->setProperty("message", text);
        return view;
    }

    void updateConfig(const QJsonObject &config) override { itemConfig = config; }
};

QJsonObject urlConfig(const QString &url)
{
    return QJsonObject{{QStringLiteral("url"), url}};
}

} // namespace

class TestUrls : public QObject
{
    Q_OBJECT

private slots:
    void pageAddress_data()
    {
        QTest::addColumn<QJsonObject>("itemConfig");
        QTest::addColumn<QUrl>("url");

        QTest::newRow("https") << urlConfig("https://example.com/a?b=c#d")
                               << QUrl(QStringLiteral("https://example.com/a?b=c#d"));
        QTest::newRow("http") << urlConfig("http://localhost:8080/") << QUrl(QStringLiteral("http://localhost:8080/"));
        QTest::newRow("upper case scheme") << urlConfig("HTTPS://example.com/")
                                           << QUrl(QStringLiteral("https://example.com/"));
        QTest::newRow("spaces") << urlConfig(" https://example.com/ ") << QUrl(QStringLiteral("https://example.com/"));
        QTest::newRow("missing") << QJsonObject() << QUrl();
        QTest::newRow("empty") << urlConfig(QString()) << QUrl();
        QTest::newRow("no scheme") << urlConfig("example.com") << QUrl();
        QTest::newRow("file") << urlConfig("file:///C:/page.html") << QUrl();
        QTest::newRow("data") << urlConfig("data:text/html,hello") << QUrl();
        QTest::newRow("no host") << urlConfig("https://") << QUrl();
    }

    void pageAddress()
    {
        QFETCH(QJsonObject, itemConfig);
        QFETCH(QUrl, url);
        QCOMPARE(pageUrl(itemConfig), url);
    }

    void typeDeclaresRenderingAndConnection()
    {
        mirante::ItemTypeRegistry registry;
        registry.registerModule(QStringLiteral("web"), &mirante::modules::web::registerModule);

        const mirante::ItemType &type = registry.type(QStringLiteral("web.page"));
        QVERIFY(type.rendering() == mirante::Rendering::Web);
        QVERIFY(type.connectionKinds().isEmpty());
    }

    void viewShowsPageOrMessage_data()
    {
        QTest::addColumn<QJsonObject>("itemConfig");
        QTest::addColumn<QUrl>("url");
        QTest::addColumn<QString>("message");

        const QString noAddress = QStringLiteral("Set the address in the item's settings.");
        QTest::newRow("address") << urlConfig("https://example.com/") << QUrl(QStringLiteral("https://example.com/"))
                                 << QString();
        QTest::newRow("missing") << QJsonObject() << QUrl() << noAddress;
        QTest::newRow("not http") << urlConfig("file:///C:/page.html") << QUrl() << noAddress;
    }

    void viewShowsPageOrMessage()
    {
        QFETCH(QJsonObject, itemConfig);
        QFETCH(QUrl, url);
        QFETCH(QString, message);

        mirante::ItemTypeRegistry registry;
        registry.registerModule(QStringLiteral("web"), &mirante::modules::web::registerModule);

        FakeContext context;
        context.itemConfig = itemConfig;

        QWidget parent;
        std::unique_ptr<QWidget> view(registry.type(QStringLiteral("web.page")).createView(context, &parent));
        QVERIFY(view);
        QCOMPARE(view->parentWidget(), &parent);
        QCOMPARE(view->property("url").toUrl(), url);
        QCOMPARE(view->property("message").toString(), message);
    }
};

QTEST_MAIN(TestUrls)
#include "tst_urls.moc"
