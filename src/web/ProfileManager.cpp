#include "web/ProfileManager.h"

#include "core/Entities.h"

#include <QApplication>
#include <QCryptographicHash>
#include <QDebug>
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

// Whether s can name a folder inside another without reaching outside it.
bool isPlainName(const QString &s)
{
    static const QRegularExpression plain(QStringLiteral("^[A-Za-z0-9_-]+$"));
    return plain.match(s).hasMatch();
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
void removeFolderSoon(QObject *owner, const QString &path, int attemptsLeft, std::function<void()> done)
{
    QDir dir(path);
    if (!dir.exists() || dir.removeRecursively() || attemptsLeft <= 1) {
        if (done)
            done();
        return;
    }
    QTimer::singleShot(kRemoveIntervalMs, owner, [owner, path, attemptsLeft, done = std::move(done)]() mutable {
        removeFolderSoon(owner, path, attemptsLeft - 1, std::move(done));
    });
}

} // namespace

struct ProfileManager::Private
{
    QString root;
    // The open workspace's folder under root, from setWorkspace(). Empty
    // keeps the profiles directly in root.
    QString workspace;
    // Loaded profiles by their folder. The manager deletes them.
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
    // Folders of profiles being cleared, and the temporary profile that
    // stands in for them meanwhile.
    QSet<QString> clearingFolders;
    QWebEngineProfile *standIn = nullptr;
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
    delete d->standIn;
    d->standIn = nullptr;
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

// Returns the profile kept in path, making it on first use. Its Qt storage
// name must be unique in the process, so it names the workspace too.
static QWebEngineProfile *profileAt(ProfileManager *manager, QHash<QString, QWebEngineProfile *> &profiles,
                                    const QString &path, const QString &qtName)
{
    if (QWebEngineProfile *existing = profiles.value(path))
        return existing;

    QDir().mkpath(path);
    auto *profile = new QWebEngineProfile(qtName, manager);
    // A named profile makes an empty folder under the default data path
    // before it can be moved. Qt 6.8 has no way to name it without that. The
    // folder stays: Chromium keeps using that path for some storage, and on
    // Windows it stops the app if the folder is gone.
    profile->setPersistentStoragePath(path);
    profile->setCachePath(QDir(path).filePath(QStringLiteral("cache")));
    profile->setPersistentCookiesPolicy(QWebEngineProfile::ForcePersistentCookies);
    profile->setPersistentPermissionsPolicy(QWebEngineProfile::PersistentPermissionsPolicy::StoreOnDisk);
    profile->setHttpUserAgent(plainUserAgent(profile->httpUserAgent()));
    profile->setDownloadPath(downloadFolder());
    QObject::connect(profile, &QWebEngineProfile::downloadRequested, manager, &askWhereToSave);
    profiles.insert(path, profile);
    return profile;
}

QString ProfileManager::storageName(const QString &profileId)
{
    // Ids come from synced data, so one that is not a plain name, and could
    // reach outside the profiles folder, uses Default instead.
    const QString id = isPlainName(profileId) ? profileId : kDefaultProfile;
    return QStringLiteral("p-%1").arg(id);
}

// The folder name under root for a workspace key.
static QString workspaceFolder(const QString &key)
{
    if (key.isEmpty() || isPlainName(key))
        return key;
    // A folder path, or an id from synced data that could reach outside root.
    const QByteArray hash = QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha1).toHex();
    return QStringLiteral("h-") + QString::fromLatin1(hash.left(16));
}

void ProfileManager::setWorkspace(const QString &key)
{
    d->workspace = workspaceFolder(key);
}

QString ProfileManager::workspace() const { return d->workspace; }

QWebEngineProfile *ProfileManager::profile(const QString &profileId)
{
    const QString name = storageName(profileId.isEmpty() ? kDefaultProfile : profileId);
    const QString path = storagePath(name);
    // A profile being cleared must not be made again over its folder. A
    // profile without a storage name is off the record: it keeps nothing.
    if (d->clearingFolders.contains(path)) {
        if (!d->standIn) {
            d->standIn = new QWebEngineProfile(this);
            d->standIn->setHttpUserAgent(plainUserAgent(d->standIn->httpUserAgent()));
        }
        return d->standIn;
    }
    const QString qtName = d->workspace.isEmpty() ? name : d->workspace + QLatin1Char('_') + name;
    return profileAt(this, d->profiles, path, qtName);
}

QWebEngineProfile *ProfileManager::appProfile()
{
    // Built-in panels belong to the app, not to a workspace.
    const QString name = QStringLiteral("app");
    return profileAt(this, d->profiles, QDir(d->root).filePath(name), name);
}

void ProfileManager::removeFolder(const QString &path, std::function<void()> done)
{
    if (d->pendingFolders.contains(path))
        return;
    d->pendingFolders.append(path);
    // Once the folder is gone, it can be removed again later, such as a
    // profile cleared twice.
    auto finished = [this, path, done = std::move(done)] {
        d->pendingFolders.removeAll(path);
        if (done)
            done();
    };

    QWebEngineProfile *profile = d->profiles.take(path);
    if (!profile) {
        removeFolderSoon(this, path, kRemoveAttempts, std::move(finished));
        return;
    }
    // A loaded profile: clear what can be cleared now, then release it. Its
    // folder is removed once the profile is gone.
    connect(profile, &QObject::destroyed, this, [this, path, finished = std::move(finished)] {
        removeFolderSoon(this, path, kRemoveAttempts, finished);
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
    if (profileId.isEmpty())
        return;
    // An id that is not a plain name maps to Default's folder, which is not
    // its own to remove.
    const QString name = storageName(profileId);
    if (name == storageName(kDefaultProfile) && profileId != kDefaultProfile)
        return;
    removeFolder(storagePath(name));
}

void ProfileManager::clearProfile(const QString &profileId)
{
    const QString id = profileId.isEmpty() ? QString(kDefaultProfile) : profileId;
    const QString path = storagePath(storageName(id));
    if (d->clearingFolders.contains(path))
        return;
    d->clearingFolders.insert(path);
    emit profileClearing(id);
    // The views delete their pages later, and the profile must go after them.
    QTimer::singleShot(0, this, [this, id, path] {
        removeFolder(path, [this, id, path] {
            d->clearingFolders.remove(path);
            emit profileCleared(id);
        });
    });
}

bool ProfileManager::isClearing(const QString &profileId) const
{
    return d->clearingFolders.contains(storagePath(storageName(profileId.isEmpty() ? kDefaultProfile : profileId)));
}

void ProfileManager::removeOldProfiles()
{
    const QStringList folders = QDir(d->root).entryList({QStringLiteral("g-*"), QStringLiteral("shared-*")},
                                                        QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &folder : folders)
        removeFolder(QDir(d->root).filePath(folder));
}

int ProfileManager::adoptRootProfiles()
{
    return adoptProfilesIn(d->root);
}

int ProfileManager::adoptProfilesOf(const QString &workspaceId)
{
    const QString folder = workspaceFolder(workspaceId);
    if (folder.isEmpty() || folder == d->workspace)
        return 0;
    const QString source = QDir(d->root).filePath(folder);
    const int moved = adoptProfilesIn(source);
    // The old workspace's folder goes once nothing is left in it.
    QDir().rmdir(source);
    return moved;
}

int ProfileManager::adoptProfilesIn(const QString &source)
{
    if (d->workspace.isEmpty())
        return 0;
    const QDir root(d->root);
    const QDir sourceDir(source);
    const QString target = root.filePath(d->workspace);
    int moved = 0;
    for (const QString &folder : sourceDir.entryList({QStringLiteral("p-*")}, QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QString from = sourceDir.filePath(folder);
        const QString to = QDir(target).filePath(folder);
        // A profile already loaded from there, or a folder the workspace
        // already has, stays as it is.
        if (d->profiles.contains(from) || QFileInfo::exists(to))
            continue;
        QDir().mkpath(target);
        if (QDir().rename(from, to))
            ++moved;
        else
            qWarning().noquote() << "Could not move the web logins in" << from << "to" << to;
    }
    return moved;
}

QString ProfileManager::storagePath(const QString &storageName) const
{
    const QDir root(d->root);
    return d->workspace.isEmpty() ? root.filePath(storageName)
                                  : QDir(root.filePath(d->workspace)).filePath(storageName);
}

} // namespace mirante
