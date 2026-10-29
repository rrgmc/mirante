// Tests WebPanel and its parts without the network: how new windows and
// permissions are handled, the login dialog's size, a panel loading pages with
// its toolbar, popups, the permission bar and a failed load.
#include "web/AddressField.h"
#include "web/LoginDialog.h"
#include "web/WebPage.h"
#include "web/WebPanel.h"

#include <QApplication>
#include <QDesktopServices>
#include <QFile>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QScopeGuard>
#include <QScreen>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineView>

#include <memory>

using namespace mirante;

namespace {

// Starting Chromium for the first page takes a while.
constexpr int kTimeout = 20000;

using Type = QWebEnginePermission::PermissionType;

} // namespace

// Receives the URLs that QDesktopServices would open in the external browser.
class UrlCatcher : public QObject
{
    Q_OBJECT

public:
    QList<QUrl> urls;

public slots:
    void openUrl(const QUrl &url) { urls.append(url); }
};

class TestWebPanel : public QObject
{
    Q_OBJECT

private:
    // Off the record, so the test writes nothing to disk. It outlives every
    // panel, which each test deletes before it ends.
    std::unique_ptr<QWebEngineProfile> profile;

    // A shown panel. The load it starts has not ended yet.
    std::unique_ptr<WebPanel> makePanel(const QUrl &url)
    {
        auto panel = std::make_unique<WebPanel>(profile.get(), url);
        panel->resize(600, 400);
        panel->show();
        return panel;
    }

    // Waits for the next load that a spy on loadFinished sees, and returns
    // whether it succeeded.
    static bool loaded(QSignalSpy &spy)
    {
        if (spy.isEmpty() && !spy.wait(kTimeout))
            return false;
        return spy.takeFirst().at(0).toBool();
    }

    static QUrl writePage(const QString &path, const QByteArray &content)
    {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly))
            return {};
        file.write(content);
        return QUrl::fromLocalFile(path);
    }

    static void runJavaScript(QWebEnginePage *page, const QString &script)
    {
        bool done = false;
        page->runJavaScript(script, [&done](const QVariant &) { done = true; });
        QTRY_VERIFY_WITH_TIMEOUT(done, kTimeout);
    }

private slots:
    void initTestCase() { profile = std::make_unique<QWebEngineProfile>(); }

    void cleanupTestCase()
    {
        // Pages release their resources later; let them go before the profile.
        QTest::qWait(100);
        profile.reset();
    }

    void classifyNewWindow_data()
    {
        QTest::addColumn<QWebEnginePage::WebWindowType>("type");
        QTest::addColumn<bool>("userInitiated");
        QTest::addColumn<NewWindow>("expected");
        for (const bool user : {true, false}) {
            const char *how = user ? "user" : "script";
            QTest::addRow("window %s", how) << QWebEnginePage::WebBrowserWindow << user << NewWindow::ExternalBrowser;
            QTest::addRow("tab %s", how) << QWebEnginePage::WebBrowserTab << user << NewWindow::ExternalBrowser;
            QTest::addRow("background tab %s", how)
                << QWebEnginePage::WebBrowserBackgroundTab << user << NewWindow::ExternalBrowser;
            QTest::addRow("dialog %s", how) << QWebEnginePage::WebDialog << user << NewWindow::LoginPopup;
        }
    }

    void classifyNewWindow()
    {
        QFETCH(QWebEnginePage::WebWindowType, type);
        QFETCH(bool, userInitiated);
        QFETCH(NewWindow, expected);
        QCOMPARE(mirante::classifyNewWindow(type, userInitiated), expected);
    }

    void windowTypeOfDestination()
    {
        QCOMPARE(windowTypeOf(QWebEngineNewWindowRequest::InNewWindow), QWebEnginePage::WebBrowserWindow);
        QCOMPARE(windowTypeOf(QWebEngineNewWindowRequest::InNewTab), QWebEnginePage::WebBrowserTab);
        QCOMPARE(windowTypeOf(QWebEngineNewWindowRequest::InNewDialog), QWebEnginePage::WebDialog);
        QCOMPARE(windowTypeOf(QWebEngineNewWindowRequest::InNewBackgroundTab),
                 QWebEnginePage::WebBrowserBackgroundTab);
    }

    void permissionHandlingByType()
    {
        QCOMPARE(permissionHandling(Type::ClipboardReadWrite), PermissionHandling::Ask);
        QCOMPARE(permissionHandling(Type::Notifications), PermissionHandling::Ask);
        for (const Type type : {Type::MediaAudioCapture, Type::MediaVideoCapture, Type::MediaAudioVideoCapture,
                                Type::DesktopVideoCapture, Type::DesktopAudioVideoCapture, Type::Geolocation,
                                Type::MouseLock, Type::LocalFontsAccess, Type::Unsupported})
            QCOMPARE(permissionHandling(type), PermissionHandling::Deny);
    }

    void loginDialogSizes()
    {
        const QSize screen(1920, 1080);
        QCOMPARE(loginDialogSize(QSize(), screen), QSize(500, 600));
        QCOMPARE(loginDialogSize(QSize(400, 300), screen), QSize(500, 600));
        QCOMPARE(loginDialogSize(QSize(800, 700), screen), QSize(800, 700));
        QCOMPARE(loginDialogSize(QSize(3000, 2000), screen), screen);
        QCOMPARE(loginDialogSize(QSize(400, 300), QSize(450, 550)), QSize(450, 550));
        QCOMPARE(loginDialogSize(QSize(800, 700), QSize()), QSize(800, 700));
    }

    void loadsAndShowsAddress()
    {
        const QUrl url(QStringLiteral("data:text/plain,hello"));
        auto panel = makePanel(url);
        QSignalSpy loads(panel->view(), &QWebEngineView::loadFinished);

        QWidget *toolbar = panel->toolbar();
        QVERIFY(toolbar);
        toolbar->resize(500, toolbar->sizeHint().height());
        toolbar->show();
        auto *back = toolbar->findChild<QToolButton *>(QStringLiteral("backButton"));
        auto *address = toolbar->findChild<AddressField *>(QStringLiteral("address"));
        QVERIFY(back);
        QVERIFY(address);
        QVERIFY(toolbar->findChild<QToolButton *>(QStringLiteral("reloadButton")));
        QVERIFY(toolbar->findChild<QToolButton *>(QStringLiteral("openInBrowserButton")));

        QVERIFY(!back->isEnabled());
        QVERIFY(loaded(loads));
        QCOMPARE(address->address(), url.toString());
        QCOMPARE(address->text(), url.toString());
        QVERIFY(!back->isEnabled());
    }

    void backAfterSecondLoad()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QUrl first = writePage(dir.filePath(QStringLiteral("one.html")), "<p>one</p>");
        const QUrl second = writePage(dir.filePath(QStringLiteral("two.html")), "<p>two</p>");
        auto panel = makePanel(first);
        QSignalSpy loads(panel->view(), &QWebEngineView::loadFinished);
        auto *back = panel->toolbar()->findChild<QToolButton *>(QStringLiteral("backButton"));
        QVERIFY(loaded(loads));
        QVERIFY(!back->isEnabled());

        panel->view()->setUrl(second);
        QVERIFY(loaded(loads));
        QTRY_VERIFY(back->isEnabled());

        back->click();
        QVERIFY(loaded(loads));
        QCOMPARE(panel->view()->url(), first);
        QTRY_VERIFY(!back->isEnabled());
    }

    void longAddressIsElided()
    {
        AddressField field;
        field.resize(120, field.sizeHint().height());
        field.show();
        const QString address = QStringLiteral("https://example.invalid/") + QString(200, QLatin1Char('a'));
        field.setAddress(address);
        QCOMPARE(field.address(), address);
        QVERIFY(field.text().contains(QChar(0x2026)));
        QCOMPARE(field.toolTip(), address);
    }

    void newWindowOpensInBrowser()
    {
        // The handler stands in for the external browser, so nothing is fetched.
        UrlCatcher catcher;
        QDesktopServices::setUrlHandler(QStringLiteral("http"), &catcher, "openUrl");
        auto cleanup = qScopeGuard([] { QDesktopServices::unsetUrlHandler(QStringLiteral("http")); });

        auto panel = makePanel(QUrl(QStringLiteral("data:text/plain,opener")));
        QSignalSpy loads(panel->view(), &QWebEngineView::loadFinished);
        QVERIFY(loaded(loads));
        QWebEnginePage *page = panel->view()->page();
        // A script may open windows only after a click, unless this is set.
        page->settings()->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, true);

        runJavaScript(page, QStringLiteral("window.open('http://example.invalid/next', '_blank'); 0"));
        QTRY_COMPARE_WITH_TIMEOUT(catcher.urls.size(), 1, kTimeout);
        QCOMPARE(catcher.urls.first(), QUrl(QStringLiteral("http://example.invalid/next")));
        QVERIFY(!panel->findChild<LoginDialog *>());
    }

    void popupOpensLoginDialog()
    {
        auto panel = makePanel(QUrl(QStringLiteral("data:text/plain,opener")));
        QSignalSpy loads(panel->view(), &QWebEngineView::loadFinished);
        QVERIFY(loaded(loads));
        QWebEnginePage *page = panel->view()->page();
        page->settings()->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, true);

        runJavaScript(page, QStringLiteral("window.open('about:blank', 'login', 'popup,width=400,height=300'); 0"));
        QPointer<LoginDialog> dialog;
        QTRY_VERIFY_WITH_TIMEOUT((dialog = panel->findChild<LoginDialog *>()), kTimeout);
        QVERIFY(dialog->isVisible());
        const QSize available = dialog->screen()->availableSize();
        QCOMPARE(dialog->size(), loginDialogSize(QSize(400, 300), available));
        QCOMPARE(dialog->page()->profile(), profile.get());

        // The popup closes itself, as a sign-in page does when it is done.
        runJavaScript(dialog->page(), QStringLiteral("window.close(); 0"));
        QTRY_VERIFY_WITH_TIMEOUT(!dialog, kTimeout);
    }

    void permissionBarAsks()
    {
        // Notifications need a secure origin. The page's content is given, and
        // only its address is https, so nothing is fetched.
        auto panel = makePanel(QUrl());
        QWebEnginePage *page = panel->view()->page();
        QSignalSpy loads(page, &QWebEnginePage::loadFinished);
        page->setHtml(QStringLiteral("<p>notify</p>"), QUrl(QStringLiteral("https://example.invalid/")));
        QTRY_VERIFY_WITH_TIMEOUT(!loads.isEmpty() && loads.last().at(0).toBool(), kTimeout);

        auto *bar = panel->findChild<QWidget *>(QStringLiteral("permissionBar"));
        auto *text = panel->findChild<QLabel *>(QStringLiteral("permissionText"));
        auto *deny = panel->findChild<QPushButton *>(QStringLiteral("denyPermission"));
        QVERIFY(bar);
        QVERIFY(text);
        QVERIFY(deny);
        QVERIFY(!bar->isVisible());

        runJavaScript(page, QStringLiteral("Notification.requestPermission(); 0"));
        QTRY_VERIFY_WITH_TIMEOUT(bar->isVisible(), kTimeout);
        QVERIFY(text->text().contains(QStringLiteral("example.invalid")));
        QVERIFY(text->text().contains(QStringLiteral("notifications")));

        deny->click();
        QVERIFY(!bar->isVisible());
    }

    void failedLoadShowsMessage()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("missing.html"));
        auto panel = makePanel(QUrl::fromLocalFile(path));
        QSignalSpy loads(panel->view(), &QWebEngineView::loadFinished);

        auto *failure = panel->findChild<QWidget *>(QStringLiteral("loadFailure"));
        auto *failureText = panel->findChild<QLabel *>(QStringLiteral("failureText"));
        auto *reload = panel->findChild<QPushButton *>(QStringLiteral("failureReload"));
        QVERIFY(failure);
        QVERIFY(failureText);
        QVERIFY(reload);
        QVERIFY(!loaded(loads));
        QVERIFY(failure->isVisible());
        QVERIFY(!panel->view()->isVisible());
        QVERIFY(failureText->text().contains(QStringLiteral("missing.html")));

        // Reloading once the file exists shows the page again.
        QVERIFY(writePage(path, "<p>found</p>").isValid());
        reload->click();
        QVERIFY(loaded(loads));
        QVERIFY(panel->view()->isVisible());
        QVERIFY(!failure->isVisible());
    }

    // A page whose process ends shows a message with Reload, not a blank page.
    void endedPageProcessShowsMessage()
    {
        auto panel = makePanel(QUrl(QStringLiteral("data:text/html,<p>alive</p>")));
        QSignalSpy loads(panel->view(), &QWebEngineView::loadFinished);
        QVERIFY(loaded(loads));
        QSignalSpy ended(panel->view()->page(), &QWebEnginePage::renderProcessTerminated);

        // Chromium's address for ending the page's process on purpose.
        panel->view()->page()->load(QUrl(QStringLiteral("chrome://kill")));
        QTRY_VERIFY_WITH_TIMEOUT(!ended.isEmpty(), kTimeout);

        auto *failure = panel->findChild<QWidget *>(QStringLiteral("loadFailure"));
        auto *failureText = panel->findChild<QLabel *>(QStringLiteral("failureText"));
        QVERIFY(failure);
        QVERIFY(failureText);
        QTRY_VERIFY(failure->isVisible());
        QVERIFY(failureText->text().contains(QStringLiteral("stopped")));
    }
};

int main(int argc, char *argv[])
{
    // Qt WebEngine needs shared OpenGL contexts, set before the application.
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    // Without a display, Chromium's GPU process can fail a check when a view
    // goes away. Pages render in software instead.
    if (qEnvironmentVariableIsEmpty("QTWEBENGINE_CHROMIUM_FLAGS"))
        qputenv("QTWEBENGINE_CHROMIUM_FLAGS", "--disable-gpu");
    QApplication app(argc, argv);
    TestWebPanel test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_web_panel.moc"
