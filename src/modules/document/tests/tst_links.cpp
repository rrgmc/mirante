// Choosing a document's format, decoding its text, and what the document
// types make of a pasted address or a dropped file. See README.md.
#include "../DocumentFormat.h"
#include "../DocumentLinks.h"

#include <QDir>
#include <QTest>

using namespace mirante::modules::document;

class TestLinks : public QObject
{
    Q_OBJECT

private slots:
    void formatFromName()
    {
        QCOMPARE(mirante::modules::document::formatFromName(QStringLiteral("README.md")), Format::Markdown);
        QCOMPARE(mirante::modules::document::formatFromName(QStringLiteral("a.MARKDOWN")), Format::Markdown);
        QCOMPARE(mirante::modules::document::formatFromName(QStringLiteral("notes.txt")), Format::Text);
        QCOMPARE(mirante::modules::document::formatFromName(QStringLiteral("build.log")), Format::Text);
        QCOMPARE(mirante::modules::document::formatFromName(QStringLiteral("spec.PDF")), Format::Pdf);
        QCOMPARE(mirante::modules::document::formatFromName(QStringLiteral("image.png")), Format::Unknown);
        QCOMPARE(mirante::modules::document::formatFromName(QStringLiteral("Makefile")), Format::Unknown);
    }

    void formatFromContentType()
    {
        QCOMPARE(mirante::modules::document::formatFromContentType(QStringLiteral("text/markdown; charset=utf-8")), Format::Markdown);
        QCOMPARE(mirante::modules::document::formatFromContentType(QStringLiteral("application/pdf")), Format::Pdf);
        QCOMPARE(mirante::modules::document::formatFromContentType(QStringLiteral("text/plain; charset=utf-8")), Format::Text);
        QCOMPARE(mirante::modules::document::formatFromContentType(QStringLiteral("text/html")), Format::Text);
        QCOMPARE(mirante::modules::document::formatFromContentType(QStringLiteral("image/png")), Format::Unknown);
        QCOMPARE(mirante::modules::document::formatFromContentType(QString()), Format::Unknown);
    }

    // The setting first, then the extension, then the content type: GitHub's
    // raw server sends Markdown as text/plain.
    void chooseFormatOrder()
    {
        const QString plain = QStringLiteral("text/plain; charset=utf-8");
        QCOMPARE(chooseFormat(kFormatAuto, QStringLiteral("README.md"), plain), Format::Markdown);
        QCOMPARE(chooseFormat(kFormatText, QStringLiteral("README.md"), plain), Format::Text);
        QCOMPARE(chooseFormat(kFormatPdf, QStringLiteral("README.md"), plain), Format::Pdf);
        QCOMPARE(chooseFormat(kFormatMarkdown, QStringLiteral("data.xyz"), QString()), Format::Markdown);
        QCOMPARE(chooseFormat(kFormatAuto, QStringLiteral("download"), QStringLiteral("application/pdf")), Format::Pdf);
        QCOMPARE(chooseFormat(QString(), QStringLiteral("data.xyz"), QString()), Format::Unknown);
    }

    void sizeLimits()
    {
        QCOMPARE(sizeLimit(Format::Markdown), 5LL * 1024 * 1024);
        QCOMPARE(sizeLimit(Format::Text), 5LL * 1024 * 1024);
        QCOMPARE(sizeLimit(Format::Pdf), 50LL * 1024 * 1024);
    }

    void decodesUtf8AndByteOrderMarks()
    {
        QCOMPARE(decodeText(QByteArray("ol\xc3\xa1")), QStringLiteral("olá"));
        const QByteArray utf16("\xff\xfeh\0i\0", 6);
        QCOMPARE(decodeText(utf16), QStringLiteral("hi"));
        QCOMPARE(decodeText(QByteArray("\xef\xbb\xbfhi")), QStringLiteral("hi"));
    }

    void pastedAddresses()
    {
        auto match = urlMatch(QUrl(QStringLiteral("https://raw.githubusercontent.com/rrgmc/mirante/refs/heads/master/README.md")));
        QVERIFY(match);
        QCOMPARE(match->config.value(QStringLiteral("url")).toString(),
                 QStringLiteral("https://raw.githubusercontent.com/rrgmc/mirante/refs/heads/master/README.md"));
        QCOMPARE(match->name, QStringLiteral("README.md"));
        QVERIFY(!match->generic);

        match = urlMatch(QUrl(QStringLiteral("https://example.org/files/spec.pdf?download=1")));
        QVERIFY(match);
        QCOMPARE(match->name, QStringLiteral("spec.pdf"));

        QVERIFY(!urlMatch(QUrl(QStringLiteral("https://example.org/page.html"))));
        QVERIFY(!urlMatch(QUrl(QStringLiteral("https://example.org/"))));
        QVERIFY(!urlMatch(QUrl(QStringLiteral("ftp://example.org/a.md"))));
        QVERIFY(!urlMatch(QUrl(QStringLiteral("file:///C:/a.md"))));
    }

    void gitHubBlobBecomesRaw()
    {
        const auto match = urlMatch(QUrl(QStringLiteral("https://github.com/rrgmc/mirante/blob/master/docs/README.md")));
        QVERIFY(match);
        QCOMPARE(match->config.value(QStringLiteral("url")).toString(),
                 QStringLiteral("https://raw.githubusercontent.com/rrgmc/mirante/master/docs/README.md"));
        QCOMPARE(match->name, QStringLiteral("README.md"));

        // A path with spaces stays encoded; other GitHub pages stay as they are.
        QCOMPARE(rawGitHubUrl(QUrl(QStringLiteral("https://github.com/o/r/blob/main/My%20Doc.md"))),
                 QUrl(QStringLiteral("https://raw.githubusercontent.com/o/r/main/My%20Doc.md")));
        QCOMPARE(rawGitHubUrl(QUrl(QStringLiteral("https://github.com/o/r/tree/main/docs.md"))),
                 QUrl(QStringLiteral("https://github.com/o/r/tree/main/docs.md")));
        QCOMPARE(rawGitHubUrl(QUrl(QStringLiteral("https://example.org/o/r/blob/main/a.md"))),
                 QUrl(QStringLiteral("https://example.org/o/r/blob/main/a.md")));
    }

    void pathInsideAFolder()
    {
        QCOMPARE(pathInside(QStringLiteral("/work/repo"), QStringLiteral("/work/repo/docs/a.md")).value_or(QString()),
                 QStringLiteral("docs/a.md"));
        QVERIFY(!pathInside(QStringLiteral("/work/repo"), QStringLiteral("/work/other/a.md")));
        QVERIFY(!pathInside(QStringLiteral("/work/repo"), QStringLiteral("/work/repo")));
        QVERIFY(!pathInside(QStringLiteral("/work/repo"), QStringLiteral("/work/repo-two/a.md")));
        QVERIFY(!pathInside(QString(), QStringLiteral("/work/repo/a.md")));
    }

    void droppedFiles()
    {
        mirante::FileDrop drop;
        drop.folders = {{QStringLiteral("f1"), QStringLiteral("/work/repo")}};

        drop.path = QStringLiteral("/work/repo/docs/design.md");
        auto project = projectFileMatch(drop);
        QVERIFY(project);
        QCOMPARE(project->config.value(QStringLiteral("folder")).toString(), QStringLiteral("f1"));
        QCOMPARE(project->config.value(QStringLiteral("path")).toString(), QStringLiteral("docs/design.md"));
        QCOMPARE(project->name, QStringLiteral("design.md"));
        QVERIFY(project->copyKey.isEmpty());
        QVERIFY(!workspaceFileMatch(drop));

        drop.path = QStringLiteral("/downloads/datasheet.pdf");
        QVERIFY(!projectFileMatch(drop));
        auto workspace = workspaceFileMatch(drop);
        QVERIFY(workspace);
        QCOMPARE(workspace->copyKey, QStringLiteral("file"));
        QCOMPARE(workspace->config.value(QStringLiteral("originalName")).toString(), QStringLiteral("datasheet.pdf"));
        QCOMPARE(workspace->name, QStringLiteral("datasheet.pdf"));

        drop.path = QStringLiteral("/downloads/archive.zip");
        QVERIFY(!projectFileMatch(drop));
        QVERIFY(!workspaceFileMatch(drop));
    }
};

QTEST_MAIN(TestLinks)
#include "tst_links.moc"
