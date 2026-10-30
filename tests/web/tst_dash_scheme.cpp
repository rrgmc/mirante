// Tests the dash:// scheme: which addresses it serves, its headers, and that
// only the app profile loads it. The pages come from tests/web/dash/, served
// as dash://panels/test/. See the M9 plan, part 1.
#include "web/DashSchemeHandler.h"
#include "web/ProfileManager.h"

#include <QApplication>
#include <QLibraryInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>
#include <QWebEnginePage>
#include <QWebEngineProfile>

#include <memory>

using namespace mirante;

namespace {

// Starting Chromium for the first page takes a while.
constexpr int kTimeout = 20000;

const QUrl testPage(QStringLiteral("dash://panels/test/index.html"));

// The value of a JavaScript expression in the page.
QVariant evaluate(QWebEnginePage &page, const QString &script)
{
    QVariant result;
    bool done = false;
    page.runJavaScript(script, [&](const QVariant &value) {
        result = value;
        done = true;
    });
    if (!QTest::qWaitFor([&] { return done; }, kTimeout))
        return {};
    return result;
}

} // namespace

class TestDashScheme : public QObject
{
    Q_OBJECT

private slots:
    void resourceFor_data()
    {
        QTest::addColumn<QString>("url");
        QTest::addColumn<QString>("resource");

        QTest::newRow("panel") << "dash://panels/task-summary/index.html" << ":/panels/task-summary/index.html";
        QTest::newRow("common") << "dash://panels/common/dash.js" << ":/panels/common/dash.js";
        QTest::newRow("channel") << "dash://panels/common/qwebchannel.js" << ":/qtwebchannel/qwebchannel.js";
        QTest::newRow("query ignored") << "dash://panels/a/index.html?x=1" << ":/panels/a/index.html";
        QTest::newRow("other host") << "dash://other/a/index.html" << "";
        QTest::newRow("no host") << "dash:/a/index.html" << "";
        QTest::newRow("other scheme") << "qrc://panels/a/index.html" << "";
        QTest::newRow("empty path") << "dash://panels" << "";
        QTest::newRow("root") << "dash://panels/" << "";
        QTest::newRow("folder") << "dash://panels/a/" << "";
        QTest::newRow("double slash") << "dash://panels/a//index.html" << "";
        QTest::newRow("dot dot") << "dash://panels/a/../../icons/x.svg" << "";
        QTest::newRow("encoded dot dot") << "dash://panels/a/%2e%2e/%2E%2E/x.svg" << "";
        QTest::newRow("encoded slash") << "dash://panels/a%2f..%2f..%2fx.svg" << "";
        QTest::newRow("dot") << "dash://panels/./a/index.html" << "";
        QTest::newRow("backslash") << "dash://panels/a%5c..%5cx.svg" << "";
        QTest::newRow("colon") << "dash://panels/c:/x.svg" << "";
    }

    void resourceFor()
    {
        QFETCH(QString, url);
        QFETCH(QString, resource);
        const std::optional<QString> found = dash::resourceFor(QUrl(url));
        QCOMPARE(found.value_or(QString()), resource);
    }

    void contentTypes()
    {
        QCOMPARE(dash::contentTypeFor(QStringLiteral(":/panels/a/index.html")), QByteArray("text/html"));
        QCOMPARE(dash::contentTypeFor(QStringLiteral(":/panels/a/panel.js")), QByteArray("text/javascript"));
        QCOMPARE(dash::contentTypeFor(QStringLiteral(":/panels/a/panel.CSS")), QByteArray("text/css"));
        QCOMPARE(dash::contentTypeFor(QStringLiteral(":/panels/a/icon.svg")), QByteArray("image/svg+xml"));
        QVERIFY(dash::contentTypeFor(QStringLiteral(":/panels/a/data.json")).isEmpty());
        QVERIFY(dash::contentTypeFor(QStringLiteral(":/panels/a/README")).isEmpty());
    }

    void headers()
    {
        const QMultiMap<QByteArray, QByteArray> headers = dash::responseHeaders();
        const QByteArray policy = headers.value("Content-Security-Policy");
        QVERIFY(policy.startsWith("default-src dash:;"));
        QVERIFY(policy.contains("connect-src 'none'"));
        QVERIFY(policy.contains("object-src 'none'"));
        QCOMPARE(headers.value("X-Content-Type-Options"), QByteArray("nosniff"));
        QCOMPARE(headers.value("Referrer-Policy"), QByteArray("no-referrer"));
        QCOMPARE(headers.value("Cache-Control"), QByteArray("no-store"));
    }

    // The app profile serves the page and its scripts under the policy, which
    // stops the inline script.
    void appProfileLoadsPanels()
    {
        // The debug WebEngine stops on its own checks on the offscreen
        // platform. See docs/web-views.md#debug-builds.
        if (QLibraryInfo::isDebugBuild())
            QSKIP("The debug WebEngine stops on its own checks here; run this test in a release build.");
        QTemporaryDir root;
        ProfileManager profiles(root.path());
        QWebEngineProfile *app = profiles.appProfile();
        QVERIFY(app->urlSchemeHandler(dash::schemeName()));
        // Asking again installs nothing more.
        QCOMPARE(profiles.appProfile(), app);

        auto page = std::make_unique<QWebEnginePage>(app);
        QSignalSpy loaded(page.get(), &QWebEnginePage::loadFinished);
        page->load(testPage);
        QVERIFY(loaded.wait(kTimeout));
        QCOMPARE(loaded.first().first().toBool(), true);
        QCOMPARE(evaluate(*page, QStringLiteral("document.title")).toString(), QStringLiteral("Dash test"));
        QCOMPARE(evaluate(*page, QStringLiteral("window.dashTestLoaded === true")).toBool(), true);
        QCOMPARE(evaluate(*page, QStringLiteral("typeof QWebChannel")).toString(), QStringLiteral("function"));
        page.reset();
    }

    // A profile of the list has no handler, so the page does not load.
    void listProfileRefusesPanels()
    {
        if (QLibraryInfo::isDebugBuild())
            QSKIP("The debug WebEngine stops on its own checks here; run this test in a release build.");
        QTemporaryDir root;
        ProfileManager profiles(root.path());
        QWebEngineProfile *work = profiles.profile(QStringLiteral("default"));
        QVERIFY(!work->urlSchemeHandler(dash::schemeName()));

        auto page = std::make_unique<QWebEnginePage>(work);
        QSignalSpy loaded(page.get(), &QWebEnginePage::loadFinished);
        page->load(testPage);
        QVERIFY(loaded.wait(kTimeout));
        QCOMPARE(loaded.first().first().toBool(), false);
        page.reset();
    }
};

// The scheme must be registered before the application is made, so this test
// has its own main().
int main(int argc, char *argv[])
{
    dash::registerScheme();
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication app(argc, argv);
    TestDashScheme test;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&test, argc, argv);
}

#include "tst_dash_scheme.moc"
