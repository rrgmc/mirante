#include "core/FolderWatcher.h"

#include "core/ConflictNames.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QTimer>

#include <algorithm>
#include <array>

namespace mirante {

namespace {

const QString kWorkspaceFile = QStringLiteral("workspace.mirante");
const QString kJson = QStringLiteral(".json");

// What a folder holds, which decides the files that count in it.
enum class FolderKind {
    Top,    // only workspace.mirante
    Entity, // only *.json
    Files,  // any file
};

struct Folder
{
    QString name; // empty for the top folder
    FolderKind kind;
};

const std::array<Folder, 6> &folders()
{
    static const std::array<Folder, 6> all = {{
        {QString(), FolderKind::Top},
        {QStringLiteral("groups"), FolderKind::Entity},
        {QStringLiteral("projects"), FolderKind::Entity},
        {QStringLiteral("tasks"), FolderKind::Entity},
        {QStringLiteral("dashboards"), FolderKind::Entity},
        {QStringLiteral("files"), FolderKind::Files},
    }};
    return all;
}

// The temporary file of QSaveFile: the real name followed by a dot and six
// random letters or digits, such as "<uuid>.json.AbCdEf". The real name must
// have an extension of its own.
const QRegularExpression &saveFileTemporary()
{
    static const QRegularExpression re(QStringLiteral("\\.[0-9A-Za-z]+\\.[0-9A-Za-z]{6}$"));
    return re;
}

// Files that are still being written, or that belong to a sync tool or an
// editor rather than to the data folder.
bool isTemporary(const QString &name)
{
    // Hidden files, which include Syncthing's ".syncthing.<name>.tmp" and
    // ".~<name>" lock files.
    if (name.startsWith(QLatin1Char('.')) || name.startsWith(QLatin1Char('~')))
        return true;
    static const std::array<QLatin1String, 5> suffixes = {
        QLatin1String(".tmp"), QLatin1String(".temp"), QLatin1String(".partial"),
        QLatin1String(".part"), QLatin1String(".crdownload"),
    };
    for (const QLatin1String &suffix : suffixes) {
        if (name.endsWith(suffix, Qt::CaseInsensitive))
            return true;
    }
    return saveFileTemporary().match(name).hasMatch();
}

// Whether original, the file a conflict copy is a copy of, could be a file
// that counts in a folder of this kind.
bool originalCounts(FolderKind kind, const QString &original)
{
    switch (kind) {
    case FolderKind::Top:
        return original == kWorkspaceFile;
    case FolderKind::Entity:
        return original.endsWith(kJson);
    case FolderKind::Files:
        return true;
    }
    return false;
}

QString relativePath(const Folder &folder, const QString &name)
{
    return folder.name.isEmpty() ? name : folder.name + QLatin1Char('/') + name;
}

// What a scan remembers of each file, to tell whether it changed.
struct Stamp
{
    qint64 size = -1;
    QDateTime modified;

    bool operator==(const Stamp &other) const
    {
        return size == other.size && modified == other.modified;
    }
    bool operator!=(const Stamp &other) const { return !(*this == other); }
};

// The files that count, by relative path, and the conflict copies present.
struct Listing
{
    QHash<QString, Stamp> files;
    QStringList conflictCopies;
    QStringList subfolders; // absolute paths of the subfolders that exist
};

QStringList sorted(QStringList list)
{
    std::sort(list.begin(), list.end());
    return list;
}

} // namespace

struct FolderWatcher::Private
{
    FolderWatcher *q = nullptr;
    QDir dir;
    OwnWriteCheck isOwnWrite;
    QFileSystemWatcher *watcher = nullptr;
    QTimer timer;
    bool running = false;

    QHash<QString, Stamp> files;
    QStringList conflictCopies;

    Listing list() const;
    void updateWatches(const Listing &listing);
    void scan();
};

Listing FolderWatcher::Private::list() const
{
    Listing listing;
    for (const Folder &folder : folders()) {
        const QString path = folder.name.isEmpty() ? dir.absolutePath() : dir.absoluteFilePath(folder.name);
        const QFileInfo folderInfo(path);
        if (!folderInfo.isDir())
            continue;
        if (!folder.name.isEmpty())
            listing.subfolders.append(path);

        const QFileInfoList entries =
            QDir(path).entryInfoList(QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot);
        for (const QFileInfo &info : entries) {
            const QString name = info.fileName();
            // Temporaries first: originalOf would take "<uuid>.json.AbCdEf"
            // for a copy of "<uuid>.AbCdEf".
            if (isTemporary(name))
                continue;
            const QString original = conflicts::originalOf(name);
            if (!original.isEmpty()) {
                if (originalCounts(folder.kind, original))
                    listing.conflictCopies.append(relativePath(folder, name));
                continue;
            }
            bool counts = false;
            switch (folder.kind) {
            case FolderKind::Top:
                counts = name == kWorkspaceFile;
                break;
            case FolderKind::Entity:
                counts = name.endsWith(kJson);
                break;
            case FolderKind::Files:
                counts = true;
                break;
            }
            if (counts)
                listing.files.insert(relativePath(folder, name), Stamp{info.size(), info.lastModified()});
        }
    }
    std::sort(listing.conflictCopies.begin(), listing.conflictCopies.end());
    return listing;
}

// Watches the folders that exist, and the files that count in them. Watching
// the folders is enough for QSaveFile, which renames, but a sync tool may write
// a file in place, which some platforms report only on the file.
void FolderWatcher::Private::updateWatches(const Listing &listing)
{
    QSet<QString> wanted;
    wanted.insert(dir.absolutePath());
    for (const QString &subfolder : listing.subfolders)
        wanted.insert(subfolder);
    for (auto it = listing.files.cbegin(); it != listing.files.cend(); ++it)
        wanted.insert(dir.absoluteFilePath(it.key()));

    QSet<QString> watched;
    const QStringList current = watcher->directories() + watcher->files();
    for (const QString &path : current) {
        if (wanted.contains(path))
            watched.insert(path);
        else
            watcher->removePath(path);
    }

    QStringList toAdd;
    for (const QString &path : std::as_const(wanted)) {
        if (!watched.contains(path))
            toAdd.append(path);
    }
    if (!toAdd.isEmpty())
        watcher->addPaths(toAdd);
}

void FolderWatcher::Private::scan()
{
    if (!running)
        return;
    const Listing listing = list();
    updateWatches(listing);

    QStringList changed;
    for (auto it = listing.files.cbegin(); it != listing.files.cend(); ++it) {
        const auto previous = files.constFind(it.key());
        if (previous != files.cend() && previous.value() == it.value())
            continue;
        QFile file(dir.absoluteFilePath(it.key()));
        // A file that cannot be read is reported, so its reader sees the problem.
        if (file.open(QIODevice::ReadOnly) && isOwnWrite && isOwnWrite(it.key(), file.readAll()))
            continue;
        changed.append(it.key());
    }

    QStringList removed;
    for (auto it = files.cbegin(); it != files.cend(); ++it) {
        if (listing.files.contains(it.key()))
            continue;
        // FileWriter takes an empty content for a file it deleted.
        if (isOwnWrite && isOwnWrite(it.key(), QByteArray()))
            continue;
        removed.append(it.key());
    }

    files = listing.files;
    const bool copiesChanged = listing.conflictCopies != conflictCopies;
    conflictCopies = listing.conflictCopies;

    if (!changed.isEmpty())
        emit q->filesChanged(sorted(changed));
    if (!removed.isEmpty())
        emit q->filesRemoved(sorted(removed));
    if (copiesChanged)
        emit q->conflictCopiesChanged(conflictCopies);
}

FolderWatcher::FolderWatcher(const QString &dir, OwnWriteCheck isOwnWrite, QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
    d->q = this;
    d->dir = QDir(dir);
    d->isOwnWrite = std::move(isOwnWrite);
    d->watcher = new QFileSystemWatcher(this);
    d->timer.setSingleShot(true);
    d->timer.setInterval(300);

    connect(&d->timer, &QTimer::timeout, this, [this] { d->scan(); });
    const auto restart = [this] {
        if (d->running)
            d->timer.start();
    };
    connect(d->watcher, &QFileSystemWatcher::directoryChanged, this, restart);
    connect(d->watcher, &QFileSystemWatcher::fileChanged, this, restart);
}

FolderWatcher::~FolderWatcher() = default;

void FolderWatcher::setDelay(int ms)
{
    d->timer.setInterval(std::max(0, ms));
}

void FolderWatcher::start()
{
    stop();
    d->running = true;
    const Listing listing = d->list();
    d->updateWatches(listing);
    d->files = listing.files;
    d->conflictCopies = listing.conflictCopies;
    if (!d->conflictCopies.isEmpty())
        emit conflictCopiesChanged(d->conflictCopies);
}

void FolderWatcher::stop()
{
    d->running = false;
    d->timer.stop();
    const QStringList watched = d->watcher->directories() + d->watcher->files();
    if (!watched.isEmpty())
        d->watcher->removePaths(watched);
    d->files.clear();
    d->conflictCopies.clear();
}

} // namespace mirante
