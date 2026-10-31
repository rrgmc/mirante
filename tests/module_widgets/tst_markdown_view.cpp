// Tests MarkdownView: it loads nothing without a fetcher, asks the fetcher
// for resolved addresses, shows an image once it arrives, keeps the scroll
// position, and opens links by their scheme.
#include <mirante/widgets/MarkdownView.h>

#include <QBuffer>
#include <QDesktopServices>
#include <QImage>
#include <QScrollBar>
#include <QTest>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextFragment>
#include <QTextImageFormat>

#include <functional>
#include <utility>
#include <vector>

using namespace mirante::widgets;

namespace {

QByteArray pngOf(const QSize &size)
{
    QImage image(size, QImage::Format_ARGB32);
    image.fill(Qt::red);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes;
}

// The size of the first image the document lays out, or an empty size.
QSize firstImageSize(MarkdownView &view)
{
    QTextDocument *document = view.document();
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextCharFormat format = it.fragment().charFormat();
            if (!format.isImageFormat())
                continue;
            const QVariant resource = document->resource(QTextDocument::ImageResource,
                                                         QUrl(format.toImageFormat().name()));
            return resource.value<QImage>().size();
        }
    }
    return {};
}

} // namespace

class TestMarkdownView : public QObject
{
    Q_OBJECT

public slots:
    void openUrl(const QUrl &url) { m_opened.append(url); }

private slots:
    void init()
    {
        m_opened.clear();
        for (const char *scheme : {"http", "https", "mailto", "file"})
            QDesktopServices::setUrlHandler(QString::fromLatin1(scheme), this, "openUrl");
    }

    void cleanup()
    {
        for (const char *scheme : {"http", "https", "mailto", "file"})
            QDesktopServices::unsetUrlHandler(QString::fromLatin1(scheme));
    }

    void rendersGitHubMarkdown()
    {
        MarkdownView view;
        view.setMarkdownText(QStringLiteral("# Title\n\n| a | b |\n|---|---|\n| 1 | 2 |\n\n- [x] done\n"));
        QVERIFY(view.toPlainText().contains(QStringLiteral("Title")));
        QVERIFY(view.toPlainText().contains(QStringLiteral("done")));
        QCOMPARE(view.markdownText().left(7), QStringLiteral("# Title"));
    }

    // Qt's Markdown importer counts HTML tags to find the end of inline
    // HTML, so a void element such as <img> without "/>" would swallow all
    // the text after it. Code keeps such tags as written.
    void textAfterVoidHtmlElementsShows()
    {
        MarkdownView view;
        view.setMarkdownText(QStringLiteral("# <img src=\"a.svg\" width=\"40\"> Title\n\n"
                                            "Hello world.\n\n"
                                            "- An item with `code` and more text.\n\n"
                                            "Line one<br>line two.\n\n"
                                            "Inline `<br>` code.\n\n"
                                            "```\n<img src=\"x.png\">\n```\n\n"
                                            "The end."));
        const QString text = view.toPlainText();
        for (const char *expected : {"Title", "Hello world.", "An item with", "and more text.", "line two.",
                                     "Inline <br> code.", "<img src=\"x.png\">", "The end."})
            QVERIFY2(text.contains(QString::fromUtf8(expected)), expected);
        QCOMPARE(view.markdownText().left(9), QStringLiteral("# <img sr"));
    }

    void withoutFetcherLoadsNothing()
    {
        MarkdownView view;
        view.resize(400, 300);
        view.setMarkdownText(QStringLiteral("![x](https://example.org/a.png)"), QUrl(QStringLiteral("https://example.org/")));
        QCOMPARE(firstImageSize(view), QSize(1, 1));
    }

    void fetcherGetsResolvedAddressesAndImageShows()
    {
        MarkdownView view;
        view.resize(600, 400);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        std::vector<std::pair<QUrl, std::function<void(const QByteArray &)>>> asked;
        view.setResourceFetcher([&](const QUrl &url, QObject *, std::function<void(const QByteArray &)> done) {
            asked.emplace_back(url, std::move(done));
        });
        view.setMarkdownText(QStringLiteral("![x](img/a.png)\n\n![x](img/a.png)"),
                             QUrl(QStringLiteral("https://example.org/repo/README.md")));
        QTRY_COMPARE(asked.size(), size_t(1));
        QCOMPARE(asked[0].first, QUrl(QStringLiteral("https://example.org/repo/img/a.png")));
        QCOMPARE(firstImageSize(view), QSize(1, 1));

        asked[0].second(pngOf(QSize(40, 30)));
        QTRY_COMPARE(firstImageSize(view), QSize(40, 30));
    }

    void wideImageIsScaledToTheView()
    {
        MarkdownView view;
        view.resize(300, 200);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        view.setResourceFetcher([](const QUrl &, QObject *, std::function<void(const QByteArray &)> done) {
            done(pngOf(QSize(2000, 100)));
        });
        view.setMarkdownText(QStringLiteral("![x](a.png)"), QUrl(QStringLiteral("file:///tmp/doc.md")));
        QTRY_VERIFY(firstImageSize(view).width() > 1);
        QVERIFY(firstImageSize(view).width() < 300);
    }

    void failedImageStaysBlankAndIsNotAskedAgain()
    {
        MarkdownView view;
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        int asks = 0;
        view.setResourceFetcher([&](const QUrl &, QObject *, std::function<void(const QByteArray &)> done) {
            ++asks;
            done({});
        });
        view.setMarkdownText(QStringLiteral("![x](a.png)"), QUrl(QStringLiteral("https://example.org/")));
        view.setMarkdownText(QStringLiteral("![x](a.png)\n\nmore"), QUrl(QStringLiteral("https://example.org/")));
        QCOMPARE(asks, 1);
        QCOMPARE(firstImageSize(view), QSize(1, 1));
    }

    void sameTextKeepsTheScrollPosition()
    {
        MarkdownView view;
        view.resize(300, 100);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        QString text;
        for (int i = 0; i < 200; ++i)
            text += QStringLiteral("line %1\n\n").arg(i);
        view.setMarkdownText(text);
        view.verticalScrollBar()->setValue(500);
        view.setMarkdownText(text);
        QCOMPARE(view.verticalScrollBar()->value(), 500);
        view.setMarkdownText(text + QStringLiteral("end"));
        QCOMPARE(view.verticalScrollBar()->value(), 0);
    }

    void opensWebAndMailLinks()
    {
        MarkdownView view;
        view.setMarkdownText(QStringLiteral("x"), QUrl(QStringLiteral("https://example.org/repo/README.md")));
        emit view.anchorClicked(QUrl(QStringLiteral("https://other.org/")));
        emit view.anchorClicked(QUrl(QStringLiteral("mailto:a@example.org")));
        emit view.anchorClicked(QUrl(QStringLiteral("docs/guide.md")));
        QCOMPARE(m_opened,
                 (QList<QUrl>{QUrl(QStringLiteral("https://other.org/")), QUrl(QStringLiteral("mailto:a@example.org")),
                              QUrl(QStringLiteral("https://example.org/repo/docs/guide.md"))}));
    }

    void localLinksOpenOnlyWhenAllowed()
    {
        MarkdownView view;
        const QUrl base = QUrl::fromLocalFile(QStringLiteral("/work/doc/README.md"));
        view.setMarkdownText(QStringLiteral("x"), base);
        emit view.anchorClicked(QUrl(QStringLiteral("guide.md")));
        QVERIFY(m_opened.isEmpty());

        view.setOpenLocalLinks(true);
        emit view.anchorClicked(QUrl(QStringLiteral("guide.md")));
        QCOMPARE(m_opened, QList<QUrl>{QUrl::fromLocalFile(QStringLiteral("/work/doc/guide.md"))});
    }

    void anchorLinksOpenNothing()
    {
        MarkdownView view;
        view.setMarkdownText(QStringLiteral("# Top\n\n[down](#top)"), QUrl(QStringLiteral("https://example.org/")));
        emit view.anchorClicked(QUrl(QStringLiteral("#top")));
        QVERIFY(m_opened.isEmpty());
    }

private:
    QList<QUrl> m_opened;
};

QTEST_MAIN(TestMarkdownView)
#include "tst_markdown_view.moc"
