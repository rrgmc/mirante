#include "core/FileWriter.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSet>
#include <QTimer>

#include <map>
#include <utility>
#include <vector>

namespace mirante {

namespace {

constexpr int kDefaultDelay = 500;

QByteArray hashOf(const QByteArray &content)
{
    return QCryptographicHash::hash(content, QCryptographicHash::Sha1);
}

// Writes content to path, creating missing folders. Returns an error message,
// or an empty string on success.
QString writeFile(const QString &path, const QByteArray &content)
{
    const QString folder = QFileInfo(path).absolutePath();
    if (!QDir().mkpath(folder))
        return QStringLiteral("Could not create the folder %1").arg(QDir::toNativeSeparators(folder));

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return file.errorString();
    if (file.write(content) != content.size()) {
        const QString message = file.errorString();
        file.cancelWriting();
        return message;
    }
    if (!file.commit())
        return file.errorString();
    return {};
}

// Deletes path. A file that is already gone counts as deleted.
QString deleteFile(const QString &path)
{
    QFile file(path);
    if (!file.exists() || file.remove())
        return {};
    return file.errorString();
}

} // namespace

struct FileWriter::Private
{
    QDir dir;
    QTimer timer;
    // Writes not done yet, by relative path. nullopt deletes the file.
    std::map<QString, std::optional<QByteArray>> pending;
    // The SHA-1 hash of what was last written to each relative path. A deleted
    // file has the hash of an empty content.
    std::map<QString, QByteArray> hashes;
    // Paths whose failure has been reported and has not succeeded since.
    QSet<QString> reported;
};

FileWriter::FileWriter(const QString &dir, QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
    d->dir = QDir(dir);
    d->timer.setSingleShot(true);
    d->timer.setInterval(kDefaultDelay);
    connect(&d->timer, &QTimer::timeout, this, &FileWriter::flush);
}

FileWriter::~FileWriter() = default;

void FileWriter::setDelay(int ms)
{
    d->timer.setInterval(ms);
}

void FileWriter::schedule(const QString &relPath, std::optional<QByteArray> content)
{
    d->pending[relPath] = std::move(content);
    d->timer.start();
}

void FileWriter::flush()
{
    d->timer.stop();

    // Take the pending writes out first, so that a slot connected to the
    // signals below can schedule new writes without disturbing this loop.
    std::map<QString, std::optional<QByteArray>> work;
    work.swap(d->pending);

    std::vector<QString> succeeded;
    std::vector<std::pair<QString, QString>> failed;
    for (auto &[relPath, content] : work) {
        const QString path = d->dir.filePath(relPath);
        const QString error = content ? writeFile(path, *content) : deleteFile(path);
        if (error.isEmpty()) {
            d->hashes[relPath] = hashOf(content.value_or(QByteArray()));
            d->reported.remove(relPath);
            succeeded.push_back(relPath);
        } else {
            // Keep it for the next flush.
            d->pending.emplace(relPath, std::move(content));
            if (!d->reported.contains(relPath)) {
                d->reported.insert(relPath);
                failed.emplace_back(relPath, error);
            }
        }
    }

    for (const auto &[relPath, message] : failed)
        emit writeFailed(relPath, message);
    for (const QString &relPath : succeeded)
        emit written(relPath);
}

bool FileWriter::pending() const
{
    return !d->pending.empty();
}

bool FileWriter::isOwnWrite(const QString &relPath, const QByteArray &content) const
{
    const auto it = d->hashes.find(relPath);
    return it != d->hashes.end() && it->second == hashOf(content);
}

} // namespace mirante
