#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QFileSystemWatcher>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <QUrl>

#include <functional>

class QNetworkReply;

namespace mirante::modules::document {

// Where a document view gets its file's bytes. Each document type has its
// own: a web address, or a local file. See docs/item-types.md#documents.
class DocumentSource : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    // Reads the file, and emits loaded(), unchanged() or failed() once.
    virtual void load() = 0;

    // The address relative images and links resolve against, and that Open
    // opens. Invalid if there is none yet.
    virtual QUrl location() const = 0;

    // The folder a relative image of a local document may be read from.
    // Empty if local images are not read.
    virtual QString localRoot() const { return {}; }

signals:
    // The file's bytes, its name, which may choose its format, and the
    // server's content type, if any.
    void loaded(const QByteArray &bytes, const QString &fileName, const QString &contentType);
    // The file has not changed since it was last loaded.
    void unchanged();
    // The file could not be read; message says why, for the user.
    void failed(const QString &message);
    // The file may have changed; the view loads it again.
    void changed();
};

// A file at a web address, downloaded without a login. A download sends the
// ETag and Last-Modified of the last answer, and stops at kReadLimit.
class UrlSource : public DocumentSource
{
    Q_OBJECT

public:
    // refreshMinutes above 0 emits changed() that often.
    UrlSource(const QUrl &url, int refreshMinutes, QObject *parent = nullptr);

    void load() override;
    QUrl location() const override { return m_url; }

private:
    void finish(QNetworkReply *reply);

    QUrl m_url;
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_reply;
    bool m_tooLarge = false;
    bool m_hasContent = false;
    QByteArray m_etag;
    QByteArray m_lastModified;
    QTimer m_refresh;
};

// A local file. Its path is asked for on each load, since it may appear
// later. It is watched, with its folder, so a change in an editor, or a
// copy that sync replaces, is read again.
class FileSource : public DocumentSource
{
    Q_OBJECT

public:
    struct Options
    {
        // The file's path, or empty when it is missing.
        std::function<QString()> path;
        // The name that chooses the format, if not the path's own.
        QString fileName;
        // The message for a missing file.
        QString missingMessage;
        // The folder relative images may be read from, or empty for none.
        QString localRoot;
        // How often a missing file is looked for again; 0 for never.
        int retryMissingMs = 0;
    };

    explicit FileSource(Options options, QObject *parent = nullptr);

    void load() override;
    QUrl location() const override;
    QString localRoot() const override { return m_options.localRoot; }

private:
    void watch(const QString &path);
    void somethingChanged();

    Options m_options;
    QString m_path;             // the path last read
    QDateTime m_modified;       // its time and size when it was read
    qint64 m_size = -1;
    bool m_missing = false;
    QFileSystemWatcher m_watcher;
    QTimer m_settle;            // gathers a burst of changes
    QTimer m_retry;
};

// Reads images for a document's Markdown: http and https addresses through
// the network, and local files only inside a root folder. Each image stops
// at 10 MB.
class ResourceFetcher : public QObject
{
public:
    explicit ResourceFetcher(QObject *parent = nullptr);

    void setLocalRoot(const QString &root) { m_localRoot = root; }
    void fetch(const QUrl &url, QObject *context, std::function<void(const QByteArray &)> done);

private:
    QNetworkAccessManager m_network;
    QString m_localRoot;
};

} // namespace mirante::modules::document
