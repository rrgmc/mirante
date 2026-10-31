#include <mirante/widgets/MarkdownView.h>

#include <QDesktopServices>
#include <QPointer>
#include <QScrollBar>
#include <QTextDocument>

namespace mirante::widgets {

namespace {

QImage blankImage()
{
    QImage blank(1, 1, QImage::Format_ARGB32);
    blank.fill(Qt::transparent);
    return blank;
}

// A link to a place in the same document, such as "#install".
bool isAnchorOnly(const QUrl &link)
{
    return link.isRelative() && link.path().isEmpty() && link.hasFragment();
}

} // namespace

MarkdownView::MarkdownView(QWidget *parent)
    : QTextBrowser(parent)
{
    setFrameShape(QFrame::NoFrame);
    setOpenLinks(false);
    setOpenExternalLinks(false);
    connect(this, &QTextBrowser::anchorClicked, this, [this](const QUrl &link) { openLink(link); });

    // Images that arrive together are laid out once.
    m_renderTimer.setSingleShot(true);
    m_renderTimer.setInterval(0);
    connect(&m_renderTimer, &QTimer::timeout, this, [this] { render(); });
}

void MarkdownView::setMarkdownText(const QString &markdown, const QUrl &base)
{
    const bool same = markdown == m_text && base == m_base;
    if (base != m_base) {
        m_images.clear();
        m_namesOf.clear();
        m_pending.clear();
    }
    m_text = markdown;
    m_base = base;
    render();
    if (!same)
        verticalScrollBar()->setValue(0);
}

void MarkdownView::setResourceFetcher(Fetcher fetcher)
{
    m_fetcher = std::move(fetcher);
}

void MarkdownView::render()
{
    const int scroll = verticalScrollBar()->value();
    // The document keeps what loadResource() answered, so an image that has
    // arrived since replaces the blank kept for it.
    for (auto it = m_images.cbegin(); it != m_images.cend(); ++it) {
        const QImage image = fitted(it.value());
        for (const QUrl &name : m_namesOf.value(it.key()))
            document()->addResource(QTextDocument::ImageResource, name, image);
    }
    setMarkdown(m_text);
    verticalScrollBar()->setValue(scroll);
}

// Never answers with nothing, because the document would then read a file
// or network path in the Markdown itself.
QVariant MarkdownView::loadResource(int type, const QUrl &name)
{
    if (type != QTextDocument::ImageResource)
        return QString(QLatin1Char(' '));
    if (!m_fetcher)
        return blankImage();

    const QUrl url = m_base.isValid() ? m_base.resolved(name) : name;
    m_namesOf[url].insert(name);
    if (const auto it = m_images.constFind(url); it != m_images.cend())
        return fitted(it.value());
    if (!m_pending.contains(url)) {
        m_pending.insert(url);
        QPointer<MarkdownView> self(this);
        const QUrl base = m_base;
        m_fetcher(url, this, [self, url, base](const QByteArray &bytes) {
            // A view that shows another document since keeps nothing of this one.
            if (!self || self->m_base != base)
                return;
            self->m_pending.remove(url);
            QImage image = QImage::fromData(bytes);
            self->m_images.insert(url, image.isNull() ? blankImage() : image);
            if (!image.isNull())
                self->m_renderTimer.start();
        });
    }
    return blankImage();
}

// An image wider than the view is scaled down to its width.
QImage MarkdownView::fitted(const QImage &image) const
{
    const int width = viewport()->width() - 2 * int(document()->documentMargin()) - 4;
    if (width < 64 || image.width() <= width)
        return image;
    return image.scaledToWidth(width, Qt::SmoothTransformation);
}

void MarkdownView::openLink(const QUrl &link)
{
    if (isAnchorOnly(link)) {
        scrollToAnchor(link.fragment());
        return;
    }
    const QUrl url = (link.isRelative() && m_base.isValid()) ? m_base.resolved(link) : link;
    const QString scheme = url.scheme();
    if (scheme == QLatin1String("http") || scheme == QLatin1String("https") || scheme == QLatin1String("mailto"))
        QDesktopServices::openUrl(url);
    else if (scheme == QLatin1String("file") && m_openLocalLinks)
        QDesktopServices::openUrl(url);
}

} // namespace mirante::widgets
