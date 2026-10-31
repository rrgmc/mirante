#include "DocumentSource.h"

#include "DocumentFormat.h"
#include "DocumentLinks.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace mirante::modules::document {

namespace {

constexpr qint64 kImageLimit = 10LL * 1024 * 1024;
constexpr int kSettleMs = 200;

QString tooLargeMessage()
{
    return QCoreApplication::translate("document", "The file is larger than %1, so it is not shown.")
        .arg(QLocale().formattedDataSize(kReadLimit));
}

} // namespace

// UrlSource

UrlSource::UrlSource(const QUrl &url, int refreshMinutes, QObject *parent)
    : DocumentSource(parent)
    , m_url(url)
{
    if (refreshMinutes > 0) {
        m_refresh.setInterval(refreshMinutes * 60 * 1000);
        connect(&m_refresh, &QTimer::timeout, this, &DocumentSource::changed);
        m_refresh.start();
    }
}

void UrlSource::load()
{
    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
        m_reply->deleteLater();
    }
    QNetworkRequest request(m_url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(30 * 1000);
    if (m_hasContent) {
        if (!m_etag.isEmpty())
            request.setRawHeader("If-None-Match", m_etag);
        if (!m_lastModified.isEmpty())
            request.setRawHeader("If-Modified-Since", m_lastModified);
    }
    m_tooLarge = false;
    QNetworkReply *reply = m_network.get(request);
    m_reply = reply;
    connect(reply, &QNetworkReply::downloadProgress, this, [this, reply](qint64 received, qint64 total) {
        if (received > kReadLimit || total > kReadLimit) {
            m_tooLarge = true;
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply] { finish(reply); });
}

void UrlSource::finish(QNetworkReply *reply)
{
    reply->deleteLater();
    if (reply != m_reply)
        return;
    m_reply = nullptr;
    if (m_tooLarge) {
        emit failed(tooLargeMessage());
        return;
    }
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status == 304) {
        emit unchanged();
        return;
    }
    if (reply->error() != QNetworkReply::NoError) {
        emit failed(QCoreApplication::translate("document", "The file could not be downloaded: %1")
                        .arg(reply->errorString()));
        return;
    }
    m_etag = reply->rawHeader("ETag");
    m_lastModified = reply->rawHeader("Last-Modified");
    m_hasContent = true;
    emit loaded(reply->readAll(), m_url.fileName(), reply->header(QNetworkRequest::ContentTypeHeader).toString());
}

// FileSource

FileSource::FileSource(Options options, QObject *parent)
    : DocumentSource(parent)
    , m_options(std::move(options))
{
    m_settle.setSingleShot(true);
    m_settle.setInterval(kSettleMs);
    connect(&m_settle, &QTimer::timeout, this, [this] { somethingChanged(); });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, &m_settle, qOverload<>(&QTimer::start));
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, &m_settle, qOverload<>(&QTimer::start));
    if (m_options.retryMissingMs > 0) {
        m_retry.setInterval(m_options.retryMissingMs);
        connect(&m_retry, &QTimer::timeout, this, [this] {
            const QString path = m_options.path();
            if (!path.isEmpty() && QFileInfo(path).isFile())
                emit changed();
        });
    }
}

QUrl FileSource::location() const
{
    return m_path.isEmpty() ? QUrl() : QUrl::fromLocalFile(m_path);
}

void FileSource::load()
{
    const QString path = m_options.path();
    const QFileInfo info(path);
    if (path.isEmpty() || !info.isFile()) {
        m_missing = true;
        if (!path.isEmpty())
            watch(path);
        if (m_options.retryMissingMs > 0)
            m_retry.start();
        emit failed(m_options.missingMessage);
        return;
    }
    m_retry.stop();
    m_missing = false;
    watch(path);
    m_path = path;
    m_modified = info.lastModified();
    m_size = info.size();
    if (info.size() > kReadLimit) {
        emit failed(tooLargeMessage());
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        emit failed(QCoreApplication::translate("document", "The file could not be read: %1").arg(file.errorString()));
        return;
    }
    const QString name = m_options.fileName.isEmpty() ? info.fileName() : m_options.fileName;
    emit loaded(file.readAll(), name, QString());
}

void FileSource::watch(const QString &path)
{
    // An editor that saves by renaming a new file over the old one drops the
    // file from the watcher, so the folder is watched too.
    const QString folder = QFileInfo(path).absolutePath();
    if (!m_watcher.directories().contains(folder) && QFileInfo(folder).isDir())
        m_watcher.addPath(folder);
    if (!m_watcher.files().contains(path) && QFileInfo(path).isFile())
        m_watcher.addPath(path);
}

void FileSource::somethingChanged()
{
    const QString path = m_options.path();
    const QFileInfo info(path);
    const bool exists = !path.isEmpty() && info.isFile();
    if (exists)
        watch(path);
    // A change of another file in the folder changes nothing here.
    if (exists && !m_missing && path == m_path && info.lastModified() == m_modified && info.size() == m_size)
        return;
    if (!exists && m_missing)
        return;
    emit changed();
}

// ResourceFetcher

ResourceFetcher::ResourceFetcher(QObject *parent)
    : QObject(parent)
{
}

void ResourceFetcher::fetch(const QUrl &url, QObject *context, std::function<void(const QByteArray &)> done)
{
    if (url.isLocalFile()) {
        const QString path = url.toLocalFile();
        if (m_localRoot.isEmpty() || !pathInside(m_localRoot, path) || QFileInfo(path).size() > kImageLimit) {
            done({});
            return;
        }
        QFile file(path);
        done(file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray());
        return;
    }
    if (url.scheme() != QLatin1String("http") && url.scheme() != QLatin1String("https")) {
        done({});
        return;
    }
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(30 * 1000);
    QNetworkReply *reply = m_network.get(request);
    connect(reply, &QNetworkReply::downloadProgress, reply, [reply](qint64 received, qint64 total) {
        if (received > kImageLimit || total > kImageLimit)
            reply->abort();
    });
    connect(reply, &QNetworkReply::finished, context, [reply, done = std::move(done)] {
        reply->deleteLater();
        done(reply->error() == QNetworkReply::NoError ? reply->readAll() : QByteArray());
    });
    // A reply whose context went first is still deleted.
    connect(context, &QObject::destroyed, reply, [reply] { reply->abort(); });
}

} // namespace mirante::modules::document
