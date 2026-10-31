#include <mirante/widgets/MarkdownView.h>

#include <QDesktopServices>
#include <QList>
#include <QPointer>
#include <QRegularExpression>
#include <QScrollBar>
#include <QStringList>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

#include <utility>

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

// Qt's Markdown importer counts HTML tags to find where inline HTML ends, so
// a void element written without "/>", such as GitHub's common
// <img src="..." width="40">, never ends, and all the text after it is lost.
// This writes such elements as self-closing, outside fenced code blocks and
// code spans, which keep the text as written.
QString closeVoidElements(const QString &markdown)
{
    static const QRegularExpression voidElement(
        QStringLiteral("<(?:area|br|col|embed|hr|img|input|link|meta|param|source|track|wbr)\\b[^<>]*>"),
        QRegularExpression::CaseInsensitiveOption);

    QStringList lines = markdown.split(QLatin1Char('\n'));
    QString fence;      // the fence of the open code block, such as "```", or empty
    for (QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (!fence.isEmpty()) {
            if (trimmed.startsWith(fence))
                fence.clear();
            continue;
        }
        if (trimmed.startsWith(QLatin1String("```")) || trimmed.startsWith(QLatin1String("~~~"))) {
            fence = trimmed.left(3);
            continue;
        }
        if (!line.contains(QLatin1Char('<')))
            continue;

        QString result;
        int i = 0;
        while (i < line.size()) {
            if (line.at(i) == QLatin1Char('`')) {
                // A code span: the same number of backticks closes it.
                int ticks = 0;
                while (i + ticks < line.size() && line.at(i + ticks) == QLatin1Char('`'))
                    ++ticks;
                const QString run(ticks, QLatin1Char('`'));
                const int close = line.indexOf(run, i + ticks);
                const int end = close < 0 ? i + ticks : close + ticks;
                result += line.mid(i, end - i);
                i = end;
                continue;
            }
            if (line.at(i) == QLatin1Char('<')) {
                const QRegularExpressionMatch match
                    = voidElement.match(line, i, QRegularExpression::NormalMatch,
                                        QRegularExpression::AnchorAtOffsetMatchOption);
                if (match.hasMatch()) {
                    QString tag = match.captured();
                    if (!tag.endsWith(QLatin1String("/>"))) {
                        tag.chop(1);
                        tag += QLatin1String(" />");
                    }
                    result += tag;
                    i += match.capturedLength();
                    continue;
                }
            }
            result += line.at(i);
            ++i;
        }
        line = result;
    }
    return lines.join(QLatin1Char('\n'));
}

// A light shade of the text colour over the base colour, for the background
// of code, in light and dark palettes alike.
QColor codeBackground(const QPalette &palette)
{
    const QColor base = palette.color(QPalette::Base);
    const QColor text = palette.color(QPalette::Text);
    constexpr float amount = 0.07f;
    return QColor::fromRgbF(base.redF() + (text.redF() - base.redF()) * amount,
                            base.greenF() + (text.greenF() - base.greenF()) * amount,
                            base.blueF() + (text.blueF() - base.blueF()) * amount);
}

// Gives code blocks and code spans a background, which Qt's Markdown
// importer does not.
void shadeCode(QTextDocument *document, const QColor &background)
{
    // The ranges are gathered first: changing a format splits fragments.
    QList<QTextBlock> codeBlocks;
    QList<std::pair<int, int>> codeSpans;     // position, length
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
        if (block.blockFormat().hasProperty(QTextFormat::BlockCodeFence)) {
            codeBlocks << block;
            continue;
        }
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (fragment.charFormat().fontFixedPitch())
                codeSpans.append({fragment.position(), fragment.length()});
        }
    }
    if (codeBlocks.isEmpty() && codeSpans.isEmpty())
        return;

    QTextCursor cursor(document);
    cursor.beginEditBlock();
    QTextBlockFormat blockFormat;
    blockFormat.setBackground(background);
    for (const QTextBlock &block : codeBlocks) {
        cursor.setPosition(block.position());
        cursor.mergeBlockFormat(blockFormat);
    }
    QTextCharFormat charFormat;
    charFormat.setBackground(background);
    for (const auto &[position, length] : codeSpans) {
        cursor.setPosition(position);
        cursor.setPosition(position + length, QTextCursor::KeepAnchor);
        cursor.mergeCharFormat(charFormat);
    }
    cursor.endEditBlock();
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
    m_prepared = closeVoidElements(markdown);
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
    setMarkdown(m_prepared);
    // Some room between the text and the cell's edges.
    document()->setDocumentMargin(10);
    shadeCode(document(), codeBackground(palette()));
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
