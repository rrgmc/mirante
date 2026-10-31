#pragma once

#include <QByteArray>
#include <QHash>
#include <QImage>
#include <QSet>
#include <QString>
#include <QTextBrowser>
#include <QTimer>
#include <QUrl>

#include <functional>

namespace mirante::widgets {

// A read-only view of Markdown, with GitHub's tables, task lists,
// strikethrough and autolinks. Web and mail links open in the external
// programs, and a link to an anchor scrolls to it. See
// docs/modules.md#the-view-kit and docs/decisions/0104-document-items.md.
//
// By default it loads nothing: an image shows as nothing, so a note cannot
// call out or read a local file. A view that shows a document gives a
// fetcher, which reads images on the view's behalf.
//
// It has no Q_OBJECT, since it adds no signals or slots: a header that gains
// Q_OBJECT later is not always picked up by AUTOMOC in an existing build
// folder. Use dynamic_cast, not qobject_cast, to find it.
class MarkdownView : public QTextBrowser
{
public:
    // Reads the resource at url, which is already resolved against the
    // base, and calls done once with its bytes, or with empty bytes if it
    // cannot. done must not be called after context is destroyed.
    using Fetcher = std::function<void(const QUrl &url, QObject *context,
                                       std::function<void(const QByteArray &bytes)> done)>;

    explicit MarkdownView(QWidget *parent = nullptr);

    // Shows markdown. Relative images and links resolve against base. The
    // scroll position is kept when the text is the same as before.
    void setMarkdownText(const QString &markdown, const QUrl &base = QUrl());
    QString markdownText() const { return m_text; }

    // Turns on loading images through fetcher. Null turns it off.
    void setResourceFetcher(Fetcher fetcher);

    // Whether a link to a local file, or a relative link in a local
    // document, opens in the external program. Off by default.
    void setOpenLocalLinks(bool open) { m_openLocalLinks = open; }

protected:
    QVariant loadResource(int type, const QUrl &name) override;

private:
    void openLink(const QUrl &link);
    void render();
    QImage fitted(const QImage &image) const;

    QString m_text;
    QString m_prepared;                     // m_text as given to Qt
    QUrl m_base;
    Fetcher m_fetcher;
    bool m_openLocalLinks = false;
    QHash<QUrl, QImage> m_images;           // fetched images, by resolved address
    QHash<QUrl, QSet<QUrl>> m_namesOf;      // the names in the text of each address
    QSet<QUrl> m_pending;
    QTimer m_renderTimer;
};

} // namespace mirante::widgets
