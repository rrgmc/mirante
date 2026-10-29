// Tests ProfileManager in a temporary folder: the storage names, folders and
// settings of the profiles, cookie isolation between groups and between
// modules, the shared profile for overridden hosts, and removing a group.
// No test uses the network.
#include "core/Entities.h"
#include "web/ProfileManager.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonObject>
#include <QNetworkCookie>
#include <QPointer>
#include <QSet>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>
#include <QWebEngineCookieStore>
#include <QWebEnginePage>
#include <QWebEngineProfile>

#include <memory>

using namespace mirante;

namespace {

const QUrl kSite(QStringLiteral("https://example.com/"));

// Opens a local page in a profile, so its cookie store is running, and
// records the names of the cookies that the store reports. Delete it before
// the profile.
class CookieNames : public QObject
{
public:
    explicit CookieNames(QWebEngineProfile *profile)
        : m_store(profile->cookieStore())
        , m_page(new QWebEnginePage(profile, this))
    {
        connect(m_store, &QWebEngineCookieStore::cookieAdded, this,
                [this](const QNetworkCookie &cookie) { names.insert(QString::fromUtf8(cookie.name())); });
    }

    bool load()
    {
        QSignalSpy loaded(m_page, &QWebEnginePage::loadFinished);
        m_page->setUrl(QUrl(QStringLiteral("data:text/html,<p>page</p>")));
        return loaded.wait(10000);
    }

    void set(const QByteArray &name)
    {
        QNetworkCookie cookie(name, "1");
        cookie.setDomain(kSite.host());
        cookie.setPath(QStringLiteral("/"));
        m_store->setCookie(cookie, kSite);
        m_store->loadAllCookies();
    }

    bool waitFor(const QString &name)
    {
        return QTest::qWaitFor([&] { return names.contains(name); }, 10000);
    }

    QSet<QString> names;

private:
    QWebEngineCookieStore *m_store;
    QWebEnginePage *m_page;
};

QString normalized(const QString &path)
{
    return QDir::cleanPath(QDir::fromNativeSeparators(path));
}

// Sets a cookie in a, and one in b, and returns the names that b's store
// reports once its own cookie arrived.
QSet<QString> cookiesSeenByOther(QWebEngineProfile *a, QWebEngineProfile *b)
{
    CookieNames inA(a);
    CookieNames inB(b);
    if (!inA.load() || !inB.load())
        return {QStringLiteral("page did not load")};

    inA.set("fromA");
    if (!inA.waitFor(QStringLiteral("fromA")))
        return {QStringLiteral("cookie never reached profile a")};

    inB.set("fromB");
    if (!inB.waitFor(QStringLiteral("fromB")))
        return {QStringLiteral("cookie never reached profile b")};
    // Give a stray cookie from a time to show up.
    QTest::qWait(300);
    return inB.names;
}

QString joined(const QSet<QString> &names)
{
    return QStringList(names.begin(), names.end()).join(u',');
}

} // namespace

class TestProfileManager : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> root;
    std::unique_ptr<ProfileManager> manager;

private slots:
    void init()
    {
        root = std::make_unique<QTemporaryDir>();
        QVERIFY(root->isValid());
        root->setAutoRemove(false);
        manager = std::make_unique<ProfileManager>(root->path());
    }

    void cleanup()
    {
        manager.reset();
        // Chromium closes some profile files a moment after the profile goes.
        const QString path = root->path();
        QVERIFY(QTest::qWaitFor([&] { return QDir(path).removeRecursively(); }, 15000));
        root.reset();
    }

    void storageNamesAndFolders()
    {
        QWebEngineProfile *group = manager->groupProfile(QStringLiteral("A"), QStringLiteral("web"));
        QWebEngineProfile *shared = manager->sharedProfile(QStringLiteral("web"));
        QWebEngineProfile *app = manager->appProfile();
        QVERIFY(group && shared && app);

        QCOMPARE(group->storageName(), QStringLiteral("g-A-web"));
        QCOMPARE(shared->storageName(), QStringLiteral("shared-web"));
        QCOMPARE(app->storageName(), QStringLiteral("app"));

        // Made once, then reused.
        QCOMPARE(manager->groupProfile(QStringLiteral("A"), QStringLiteral("web")), group);
        QCOMPARE(manager->sharedProfile(QStringLiteral("web")), shared);
        QCOMPARE(manager->appProfile(), app);
        QVERIFY(manager->groupProfile(QStringLiteral("A"), QStringLiteral("jira")) != group);
        QVERIFY(manager->groupProfile(QStringLiteral("B"), QStringLiteral("web")) != group);

        for (QWebEngineProfile *profile : {group, shared, app}) {
            const QString path = manager->storagePath(profile->storageName());
            QCOMPARE(normalized(path), normalized(QDir(root->path()).filePath(profile->storageName())));
            QCOMPARE(normalized(profile->persistentStoragePath()), normalized(path));
            QVERIFY(normalized(profile->cachePath()).startsWith(normalized(path) + QLatin1Char('/')));
            QVERIFY(QDir(path).exists());
            QVERIFY(!profile->isOffTheRecord());
            QCOMPARE(profile->persistentCookiesPolicy(), QWebEngineProfile::ForcePersistentCookies);
            QCOMPARE(profile->persistentPermissionsPolicy(),
                     QWebEngineProfile::PersistentPermissionsPolicy::StoreOnDisk);
            QVERIFY(!profile->httpUserAgent().contains(QLatin1String("QtWebEngine")));
            QVERIFY(profile->httpUserAgent().contains(QLatin1String("Chrome/")));
        }
    }

    void cookiesStayInTheirGroup()
    {
        const QSet<QString> seen = cookiesSeenByOther(manager->groupProfile(QStringLiteral("A"), QStringLiteral("web")),
                                                      manager->groupProfile(QStringLiteral("B"), QStringLiteral("web")));
        QVERIFY2(seen.contains(QStringLiteral("fromB")), qPrintable(joined(seen)));
        QVERIFY2(!seen.contains(QStringLiteral("fromA")), qPrintable(joined(seen)));
    }

    void cookiesStayInTheirModule()
    {
        const QSet<QString> seen = cookiesSeenByOther(manager->groupProfile(QStringLiteral("A"), QStringLiteral("gitlab")),
                                                      manager->groupProfile(QStringLiteral("A"), QStringLiteral("jira")));
        QVERIFY2(seen.contains(QStringLiteral("fromB")), qPrintable(joined(seen)));
        QVERIFY2(!seen.contains(QStringLiteral("fromA")), qPrintable(joined(seen)));
    }

    void overriddenHostUsesTheSharedProfile()
    {
        Group group;
        group.id = QStringLiteral("A");
        group.profileOverrides.insert(QStringLiteral("Jira.Example.com"), QStringLiteral("shared"));

        QWebEngineProfile *shared = manager->sharedProfile(QStringLiteral("jira"));
        QWebEngineProfile *own = manager->groupProfile(QStringLiteral("A"), QStringLiteral("jira"));

        QCOMPARE(manager->profileFor(group, QStringLiteral("jira"), QUrl(QStringLiteral("https://jira.example.com/browse/X-1"))), shared);
        QCOMPARE(manager->profileFor(group, QStringLiteral("jira"), QUrl(QStringLiteral("https://JIRA.example.com/"))), shared);
        // Another host, and a subdomain, stay in the group's own profile.
        QCOMPARE(manager->profileFor(group, QStringLiteral("jira"), QUrl(QStringLiteral("https://example.com/"))), own);
        QCOMPARE(manager->profileFor(group, QStringLiteral("jira"), QUrl(QStringLiteral("https://a.jira.example.com/"))), own);
        // The shared profile belongs to the item's module.
        QCOMPARE(manager->profileFor(group, QStringLiteral("web"), QUrl(QStringLiteral("https://jira.example.com/"))),
                 manager->sharedProfile(QStringLiteral("web")));
    }

    void removeGroupRemovesEveryModulesFolder()
    {
        QWebEngineProfile *web = manager->groupProfile(QStringLiteral("A"), QStringLiteral("web"));
        QWebEngineProfile *gitlab = manager->groupProfile(QStringLiteral("A"), QStringLiteral("gitlab"));
        manager->groupProfile(QStringLiteral("B"), QStringLiteral("web"));
        manager->sharedProfile(QStringLiteral("web"));

        // Write a cookie to disk in a loaded profile. The page goes before
        // the group is removed, as the app closes a group's pages first.
        {
            CookieNames inWeb(web);
            QVERIFY(inWeb.load());
            inWeb.set("session");
            QVERIFY(inWeb.waitFor(QStringLiteral("session")));
        }

        // The folder of a module missing from this build, never loaded.
        const QString absent = manager->storagePath(QStringLiteral("g-A-absent"));
        QVERIFY(QDir().mkpath(QDir(absent).filePath(QStringLiteral("cache"))));
        QFile file(QDir(absent).filePath(QStringLiteral("Cookies")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("x");
        file.close();

        // A group whose id starts with the same letter keeps its folder.
        const QString similar = manager->storagePath(QStringLiteral("g-AB-web"));
        QVERIFY(QDir().mkpath(similar));

        QPointer<QWebEngineProfile> released(gitlab);
        manager->removeGroup(QStringLiteral("A"));

        QTRY_VERIFY_WITH_TIMEOUT(released.isNull(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!QDir(manager->storagePath(QStringLiteral("g-A-web"))).exists(), 15000);
        QTRY_VERIFY_WITH_TIMEOUT(!QDir(manager->storagePath(QStringLiteral("g-A-gitlab"))).exists(), 15000);
        QTRY_VERIFY_WITH_TIMEOUT(!QDir(absent).exists(), 15000);

        QVERIFY(QDir(manager->storagePath(QStringLiteral("g-B-web"))).exists());
        QVERIFY(QDir(manager->storagePath(QStringLiteral("shared-web"))).exists());
        QVERIFY(QDir(similar).exists());
    }

    void profilesGoWithTheManager()
    {
        QPointer<QWebEngineProfile> profile = manager->groupProfile(QStringLiteral("A"), QStringLiteral("web"));
        QVERIFY(profile);
        manager.reset();
        QVERIFY(profile.isNull());
    }
};

int main(int argc, char *argv[])
{
    // Qt WebEngine shares OpenGL contexts with the application.
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication app(argc, argv);
    TestProfileManager test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_profile_manager.moc"
