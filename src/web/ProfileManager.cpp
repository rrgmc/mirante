#include "web/ProfileManager.h"

#include "core/Entities.h"

#include <QApplication>
#include <QDir>
#include <QEventLoop>
#include <QFileDialog>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QPointer>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QWebEngineCookieStore>
#include <QWebEngineDownloadRequest>
#include <QWebEngineProfile>
#include <QWebEngineView>

#include <vector>

namespace mirante {

namespace {

// How often, and how long apart, a locked profile folder is tried again.
constexpr int kRemoveAttempts = 40;
constexpr int kRemoveIntervalMs = 250;
// How long the destructor waits for cache clears still running.
constexpr int kClearWaitMs = 5000;

// Chromium's user agent with the "QtWebEngine/<version>" token removed, so
// sites see an ordinary Chromium. Some identity providers refuse a browser
// that names itself embedded.
QString plainUserAgent(const QString &userAgent)
{
    static const QRegularExpression token(QStringLiteral("\\s*QtWebEngine/\\S+"));
    return QString(userAgent).remove(token).simplified();
}

// The folder where a download is offered first.
QString downloadFolder()
{
    const QString folder = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    return folder.isEmpty() ? QDir::homePath() : folder;
}

// Asks where to save a download, starting in the system download folder.
// Without a window to show the dialog on, as in tests, the download is
// cancelled instead of blocking.
void askWhereToSave(QWebEngineDownloadRequest *download)
{
    QWidget *window = nullptr;
    if (qobject_cast<QApplication *>(QCoreApplication::instance()) && download->page()) {
        if (QWebEngineView *view = QWebEngineView::forPage(download->page()))
            window = view->window();
    }
    if (!window) {
        download->cancel();
        return;
    }

    const QString suggested = QDir(downloadFolder()).filePath(download->downloadFileName());
    const QString path = QFileDialog::getSaveFileName(window, ProfileManager::tr("Save download"), suggested);
    // The dialog runs its own event loop, so the request may have been
    // cancelled meanwhile.
    if (download->state() != QWebEngineDownloadRequest::DownloadRequested)
        return;
    if (path.isEmpty()) {
        download->cancel();
        return;
    }
    const QFileInfo info(path);
    download->setDownloadDirectory(info.absolutePath());
    download->setDownloadFileName(info.fileName());
    download->accept();
}

// Removes a folder, trying again for a while if a file in it is still in use.
// The timer stops when the manager is destroyed.
void removeFolderSoon(QObject *owner, const QString &path, int attemptsLeft)
{
    QDir dir(path);
    if (!dir.exists() || dir.removeRecursively() || attemptsLeft <= 1)
        return;
    QTimer::singleShot(kRemoveIntervalMs, owner, [owner, path, attemptsLeft] {
        removeFolderSoon(owner, path, attemptsLeft - 1);
    });
}

} // namespace

struct ProfileManager::Private
{
    QString root;
    // Loaded profiles by storage name. The manager deletes them.
    QHash<QString, QWebEngineProfile *> profiles;
    // Removed profiles, waiting for deleteLater. Their folders are
    // removed once they are gone.
    std::vector<QPointer<QWebEngineProfile>> released;
    // Released profiles whose HTTP cache is still being cleared. Deleting a
    // profile while that runs corrupts memory, so they are deleted only
    // after clearHttpCacheCompleted.
    QSet<QWebEngineProfile *> clearing;
    // Folders still to remove when the manager is destroyed.
    QStringList pendingFolders;
};

ProfileManager::ProfileManager(const QString &root, QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
    d->root = root;
}

ProfileManager::~ProfileManager()
{
    // The profiles are deleted here, before the manager returns, and not
    // later with its other QObject children.
    for (QWebEngineProfile *profile : std::as_const(d->profiles))
        delete profile;
    d->profiles.clear();
    // Released profiles still clearing their cache cannot be deleted safely
    // yet, so wait a little for the clears to end.
    if (!d->clearing.isEmpty()) {
        QEventLoop loop;
        QTimer::singleShot(kClearWaitMs, &loop, &QEventLoop::quit);
        for (QWebEngineProfile *profile : std::as_const(d->clearing))
            connect(profile, &QWebEngineProfile::clearHttpCacheCompleted, &loop, [this, &loop] {
                if (d->clearing.isEmpty())
                    loop.quit();
            });
        if (!d->clearing.isEmpty())
            loop.exec();
    }
    for (const QPointer<QWebEngineProfile> &profile : d->released) {
        if (profile) {
            disconnect(profile.data(), nullptr, this, nullptr);
            delete profile.data();
        }
    }
    d->released.clear();
    // One last try for folders of removed profiles. Ones still locked stay.
    for (const QString &path : std::as_const(d->pendingFolders))
        QDir(path).removeRecursively();
}

// Returns the profile with this storage name, making it on first use.
static QWebEngineProfile *profileNamed(ProfileManager *manager, QHash<QString, QWebEngineProfile *> &profiles,
                                       const QString &name)
{
    if (QWebEngineProfile *existing = profiles.value(name))
        return existing;

    const QString path = manager->storagePath(name);
    QDir().mkpath(path);
    auto *profile = new QWebEngineProfile(name, manager);
    // A named profile makes an empty folder under the default data path
    // before it can be moved. Qt 6.8 has no way to name it without that, so
    // the empty folders are removed again. rmdir leaves folders that are not
    // empty alone.
    const QString defaultPath = profile->persistentStoragePath();
    profile->setPersistentStoragePath(path);
    if (QDir::cleanPath(defaultPath) != QDir::cleanPath(path)) {
        QDir().rmdir(defaultPath);
        QDir().rmdir(QFileInfo(defaultPath).absolutePath());
    }
    profile->setCachePath(QDir(path).filePath(QStringLiteral("cache")));
    profile->setPersistentCookiesPolicy(QWebEngineProfile::ForcePersistentCookies);
    profile->setPersistentPermissionsPolicy(QWebEngineProfile::PersistentPermissionsPolicy::StoreOnDisk);
    profile->setHttpUserAgent(plainUserAgent(profile->httpUserAgent()));
    profile->setDownloadPath(downloadFolder());
    QObject::connect(profile, &QWebEngineProfile::downloadRequested, manager, &askWhereToSave);
    profiles.insert(name, profile);
    return profile;
}

QString ProfileManager::storageName(const QString &profileId)
{
    // Ids come from synced data, so one that is not a plain name, and could
    // reach outside the profiles folder, uses Default instead.
    static const QRegularExpression plain(QStringLiteral("^[A-Za-z0-9_-]+$"));
    const QString id = plain.match(profileId).hasMatch() ? profileId : kDefaultWebProfile;
    return QStringLiteral("p-%1").arg(id);
}

QWebEngineProfile *ProfileManager::profile(const QString &profileId)
{
    return profileNamed(this, d->profiles, storageName(profileId.isEmpty() ? kDefaultWebProfile : profileId));
}

QWebEngineProfile *ProfileManager::appProfile()
{
    return profileNamed(this, d->profiles, QStringLiteral("app"));
}

void ProfileManager::removeStorage(const QString &storageName)
{
    const QString path = storagePath(storageName);
    if (d->pendingFolders.contains(path))
        return;
    d->pendingFolders.append(path);

    QWebEngineProfile *profile = d->profiles.take(storageName);
    if (!profile) {
        removeFolderSoon(this, path, kRemoveAttempts);
        return;
    }
    // A loaded profile: clear what can be cleared now, then release it. Its
    // folder is removed once the profile is gone.
    connect(profile, &QObject::destroyed, this, [this, path] {
        removeFolderSoon(this, path, kRemoveAttempts);
    });
    d->released.emplace_back(profile);
    // The profile is deleted only once its cache is cleared; see clearing.
    d->clearing.insert(profile);
    connect(profile, &QWebEngineProfile::clearHttpCacheCompleted, this, [this, profile] {
        d->clearing.remove(profile);
        profile->deleteLater();
    }, Qt::SingleShotConnection);
    profile->cookieStore()->deleteAllCookies();
    profile->clearHttpCache();
}

void ProfileManager::removeProfile(const QString &profileId)
{
    if (profileId.isEmpty() || profileId == kDefaultWebProfile)
        return;
    const QString name = storageName(profileId);
    if (name == storageName(kDefaultWebProfile))
        return;
    removeStorage(name);
}

void ProfileManager::removeOldProfiles()
{
    const QStringList folders = QDir(d->root).entryList({QStringLiteral("g-*"), QStringLiteral("shared-*")},
                                                        QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &folder : folders)
        removeStorage(folder);
}

QString ProfileManager::storagePath(const QString &storageName) const
{
    return QDir(d->root).filePath(storageName);
}

} // namespace mirante
