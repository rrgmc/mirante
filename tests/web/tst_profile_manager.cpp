// Tests ProfileManager in a temporary folder: the storage names, folders and
// settings of the profiles, cookie isolation between profiles, removing a
// profile, and removing the profiles from before decision 0042.
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
        QWebEngineProfile *standard = manager->profile(kDefaultWebProfile);
        QWebEngineProfile *work = manager->profile(QStringLiteral("work-id"));
        QWebEngineProfile *app = manager->appProfile();
        QVERIFY(standard && work && app);

        QCOMPARE(standard->storageName(), QStringLiteral("p-default"));
        QCOMPARE(work->storageName(), QStringLiteral("p-work-id"));
        QCOMPARE(app->storageName(), QStringLiteral("app"));

        // Made once, then reused. An empty id means Default.
        QCOMPARE(manager->profile(kDefaultWebProfile), standard);
        QCOMPARE(manager->profile(QString()), standard);
        QCOMPARE(manager->profile(QStringLiteral("work-id")), work);
        QCOMPARE(manager->appProfile(), app);
        QVERIFY(manager->profile(QStringLiteral("other")) != work);

        for (QWebEngineProfile *profile : {standard, work, app}) {
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

    // Ids come from synced files, so one that could name another folder gives
    // Default.
    void oddIdsGiveDefault()
    {
        QCOMPARE(ProfileManager::storageName(QStringLiteral("../x")), QStringLiteral("p-default"));
        QCOMPARE(ProfileManager::storageName(QStringLiteral("a/b")), QStringLiteral("p-default"));
        QCOMPARE(ProfileManager::storageName(QStringLiteral("0b4c-9A_z")), QStringLiteral("p-0b4c-9A_z"));
        QCOMPARE(manager->profile(QStringLiteral("..")), manager->profile(kDefaultWebProfile));
    }

    void cookiesStayInTheirProfile()
    {
        const QSet<QString> seen = cookiesSeenByOther(manager->profile(QStringLiteral("a")),
                                                      manager->profile(QStringLiteral("b")));
        QVERIFY2(seen.contains(QStringLiteral("fromB")), qPrintable(joined(seen)));
        QVERIFY2(!seen.contains(QStringLiteral("fromA")), qPrintable(joined(seen)));
    }

    void removeProfileRemovesItsFolder()
    {
        QWebEngineProfile *work = manager->profile(QStringLiteral("work"));
        manager->profile(QStringLiteral("workshop"));
        manager->profile(kDefaultWebProfile);

        // Write a cookie to disk in the loaded profile. The page goes before
        // the profile is removed, as the app's views go first.
        {
            CookieNames inWork(work);
            QVERIFY(inWork.load());
            inWork.set("session");
            QVERIFY(inWork.waitFor(QStringLiteral("session")));
        }

        // A profile that is not loaded loses its folder too.
        const QString unloaded = manager->storagePath(QStringLiteral("p-unloaded"));
        QVERIFY(QDir().mkpath(QDir(unloaded).filePath(QStringLiteral("cache"))));

        QPointer<QWebEngineProfile> released(work);
        manager->removeProfile(QStringLiteral("work"));
        manager->removeProfile(QStringLiteral("unloaded"));
        // Default is never removed.
        manager->removeProfile(kDefaultWebProfile);
        manager->removeProfile(QString());

        QTRY_VERIFY_WITH_TIMEOUT(released.isNull(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!QDir(manager->storagePath(QStringLiteral("p-work"))).exists(), 15000);
        QTRY_VERIFY_WITH_TIMEOUT(!QDir(unloaded).exists(), 15000);

        QVERIFY(QDir(manager->storagePath(QStringLiteral("p-workshop"))).exists());
        QVERIFY(QDir(manager->storagePath(QStringLiteral("p-default"))).exists());
        QCOMPARE(manager->profile(kDefaultWebProfile)->storageName(), QStringLiteral("p-default"));
    }

    // The folders of the profiles per group and module, and the shared ones,
    // go at start. The new profiles and the app profile stay.
    void oldProfilesGo()
    {
        const QStringList old{QStringLiteral("g-A-web"), QStringLiteral("g-B-gitlab"), QStringLiteral("shared-jira")};
        for (const QString &name : old) {
            const QString path = manager->storagePath(name);
            QVERIFY(QDir().mkpath(QDir(path).filePath(QStringLiteral("cache"))));
            QFile file(QDir(path).filePath(QStringLiteral("Cookies")));
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("x");
        }
        manager->profile(QStringLiteral("g"));     // "p-g", not an old one
        manager->appProfile();

        manager->removeOldProfiles();

        for (const QString &name : old)
            QTRY_VERIFY_WITH_TIMEOUT(!QDir(manager->storagePath(name)).exists(), 15000);
        QVERIFY(QDir(manager->storagePath(QStringLiteral("p-g"))).exists());
        QVERIFY(QDir(manager->storagePath(QStringLiteral("app"))).exists());
    }

    // The manager is destroyed right after removeProfile, while the released
    // profile still clears its cache. It must wait for that, not crash.
    void managerGoesRightAfterRemoveProfile()
    {
        QPointer<QWebEngineProfile> profile = manager->profile(QStringLiteral("a"));
        {
            CookieNames inA(profile);
            QVERIFY(inA.load());
        }
        manager->removeProfile(QStringLiteral("a"));
        manager.reset();
        QVERIFY(profile.isNull());
    }

    void profilesGoWithTheManager()
    {
        QPointer<QWebEngineProfile> profile = manager->profile(QStringLiteral("a"));
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
