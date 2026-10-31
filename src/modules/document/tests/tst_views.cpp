// The document views, with a fake context: project files shown and followed
// as they change, workspace files, the messages, the editor, and downloads
// from a local HTTP server that checks the ETag. See README.md#view.
#include "../DocumentEditor.h"

#include "items/ItemTypeRegistry.h"

#include <mirante/ItemContext.h>
#include <mirante/ItemEditor.h>
#include <mirante/ItemType.h>
#include <mirante/widgets/MarkdownView.h>

#include <QBuffer>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonObject>
#include <QLabel>
#include <QPdfDocument>
#include <QPdfView>
#include <QPdfWriter>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>

#include <memory>

namespace mirante::modules::document {
void registerModule(mirante::ModuleRegistrar &registrar);
}

using mirante::widgets::MarkdownView;

namespace {

class FakeContext : public mirante::ItemContext
{
public:
    QJsonObject itemConfig;
    QHash<QString, QString> folders;    // id → local path
    QHash<QString, QString> files;      // files/ name → path
    QStringList mapped;

    QString itemName() const override { return QStringLiteral("Document"); }
    QJsonObject config() const override { return itemConfig; }
    QJsonValue projectSetting(const QString &) const override { return {}; }
    QJsonObject moduleSettings(const QString &) const override { return {}; }
    QString folderPath(const QString &id) const override { return folders.value(id); }
    void mapFolder(const QString &id) override { mapped << id; }
    QWidget *createWebView(const QUrl &, QWidget *) override { return nullptr; }
    QWebEngineProfile *profile() override { return nullptr; }
    QWidget *createMessageView(const QString &text, QWidget *parent) override
    {
        auto *label = new QLabel(text, parent);
        label->setObjectName(QStringLiteral("contextMessage"));
        return label;
    }
    void updateConfig(const QJsonObject &config) override { itemConfig = config; }
    QString filePath(const QString &name) const override
    {
        const QString path = files.value(name);
        return QFileInfo::exists(path) ? path : QString();
    }
};

// A one-request-at-a-time HTTP server. Each request is answered with the
// next reply, and its headers are kept.
class HttpServer : public QObject
{
public:
    struct Reply
    {
        int status = 200;
        QByteArray contentType;
        QByteArray body;
        QByteArray etag;
    };
    QList<Reply> replies;
    QList<QByteArray> requests;

    HttpServer()
    {
        m_server.listen(QHostAddress::LocalHost);
        connect(&m_server, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket *socket = m_server.nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead, socket, [this, socket] { read(socket); });
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            }
        });
    }

    QUrl url(const QString &path) const
    {
        return QUrl(QStringLiteral("http://127.0.0.1:%1/%2").arg(m_server.serverPort()).arg(path));
    }

private:
    void read(QTcpSocket *socket)
    {
        QByteArray &buffer = m_buffers[socket];
        buffer += socket->readAll();
        if (!buffer.contains("\r\n\r\n"))
            return;
        requests << buffer;
        buffer.clear();
        const Reply reply = replies.isEmpty() ? Reply{404, "text/plain", "missing", {}} : replies.takeFirst();
        QByteArray response = "HTTP/1.1 " + QByteArray::number(reply.status) + " X\r\n";
        if (!reply.contentType.isEmpty())
            response += "Content-Type: " + reply.contentType + "\r\n";
        if (!reply.etag.isEmpty())
            response += "ETag: " + reply.etag + "\r\n";
        response += "Content-Length: " + QByteArray::number(reply.status == 304 ? 0 : reply.body.size()) + "\r\n";
        response += "Connection: close\r\n\r\n";
        if (reply.status != 304)
            response += reply.body;
        socket->write(response);
        socket->disconnectFromHost();
    }

    QTcpServer m_server;
    QHash<QTcpSocket *, QByteArray> m_buffers;
};

bool writeFile(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

QByteArray pdfBytes()
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    {
        QPdfWriter writer(&buffer);
        QPainter painter(&writer);
        painter.drawText(100, 100, QStringLiteral("Hello"));
    }
    return bytes;
}

// The page the view shows: its Markdown, text or PDF widget, or its message.
QWidget *page(QWidget *view)
{
    auto *stack = view->findChild<QStackedWidget *>();
    return stack ? stack->currentWidget() : nullptr;
}

QString messageOf(QWidget *view)
{
    auto *label = qobject_cast<QLabel *>(page(view));
    return label ? label->text() : QString();
}

QString problemOf(QWidget *view)
{
    auto *label = view->findChild<QLabel *>(QStringLiteral("problem"));
    return label && !label->isHidden() ? label->text() : QString();
}

void click(QWidget *view, const QString &name)
{
    auto *button = view->findChild<QToolButton *>(name);
    QVERIFY(button);
    button->click();
}

} // namespace

class TestViews : public QObject
{
    Q_OBJECT

    mirante::ItemTypeRegistry registry;

    std::unique_ptr<QWidget> make(const QString &typeId, FakeContext &context)
    {
        return std::unique_ptr<QWidget>(registry.type(typeId).createView(context, nullptr));
    }

private slots:
    void initTestCase()
    {
        registry.registerModule(QStringLiteral("document"), &mirante::modules::document::registerModule);
    }

    // A Markdown file in a project folder is shown, and shown again when an
    // editor changes it, also by saving a new file over it.
    void projectFileIsFollowed()
    {
        QTemporaryDir folder;
        QVERIFY(QDir(folder.path()).mkpath(QStringLiteral("docs")));
        const QString path = folder.filePath(QStringLiteral("docs/design.md"));
        QVERIFY(writeFile(path, "# First"));
        FakeContext context;
        context.folders.insert(QStringLiteral("f1"), folder.path());
        context.itemConfig = {{QStringLiteral("folder"), QStringLiteral("f1")},
                              {QStringLiteral("path"), QStringLiteral("docs/design.md")}};
        auto view = make(QStringLiteral("document.project"), context);
        auto *markdown = dynamic_cast<MarkdownView *>(page(view.get()));
        QVERIFY(markdown);
        QCOMPARE(markdown->markdownText(), QStringLiteral("# First"));

        QTest::qWait(50);
        QVERIFY(writeFile(path, "# Second, longer"));
        QTRY_COMPARE(markdown->markdownText(), QStringLiteral("# Second, longer"));

        // As an editor that saves by renaming.
        const QString temporary = folder.filePath(QStringLiteral("docs/design.md.tmp"));
        QVERIFY(writeFile(temporary, "# Third, longer still"));
        QVERIFY(QFile::remove(path));
        QVERIFY(QFile::rename(temporary, path));
        QTRY_COMPARE(markdown->markdownText(), QStringLiteral("# Third, longer still"));
    }

    void projectMessages()
    {
        QTemporaryDir folder;
        FakeContext context;
        context.itemConfig = {{QStringLiteral("folder"), QStringLiteral("f1")},
                              {QStringLiteral("path"), QStringLiteral("missing.md")}};

        // A folder with no local path offers Choose folder….
        auto view = make(QStringLiteral("document.project"), context);
        auto *choose = view->findChild<QPushButton *>(QStringLiteral("chooseFolder"));
        QVERIFY(choose);
        choose->click();
        QCOMPARE(context.mapped, QStringList{QStringLiteral("f1")});

        context.folders.insert(QStringLiteral("f1"), folder.path());
        view = make(QStringLiteral("document.project"), context);
        QCOMPARE(messageOf(view.get()), QStringLiteral("The file is missing."));

        // The file appears later.
        QVERIFY(writeFile(folder.filePath(QStringLiteral("missing.md")), "here"));
        QTRY_VERIFY(dynamic_cast<MarkdownView *>(page(view.get())));

        context.itemConfig.insert(QStringLiteral("path"), QString());
        view = make(QStringLiteral("document.project"), context);
        QCOMPARE(view->objectName(), QStringLiteral("contextMessage"));
    }

    // Text is shown in a plain text view; the setting overrides the
    // extension; an unknown format and a large file say so.
    void formatsAndLimits()
    {
        QTemporaryDir folder;
        QVERIFY(writeFile(folder.filePath(QStringLiteral("build.log")), "line one\nline two"));
        QVERIFY(writeFile(folder.filePath(QStringLiteral("data.xyz")), "# heading"));
        QVERIFY(writeFile(folder.filePath(QStringLiteral("large.txt")), QByteArray(5 * 1024 * 1024 + 1, 'a')));
        FakeContext context;
        context.folders.insert(QStringLiteral("f1"), folder.path());

        context.itemConfig = {{QStringLiteral("folder"), QStringLiteral("f1")}, {QStringLiteral("path"), QStringLiteral("build.log")}};
        auto view = make(QStringLiteral("document.project"), context);
        auto *text = qobject_cast<QPlainTextEdit *>(page(view.get()));
        QVERIFY(text);
        QCOMPARE(text->toPlainText(), QStringLiteral("line one\nline two"));
        QVERIFY(text->isReadOnly());

        context.itemConfig.insert(QStringLiteral("path"), QStringLiteral("data.xyz"));
        view = make(QStringLiteral("document.project"), context);
        QCOMPARE(messageOf(view.get()), QStringLiteral("This file's format is not known. Choose one in the item's settings."));

        context.itemConfig.insert(QStringLiteral("format"), QStringLiteral("markdown"));
        view = make(QStringLiteral("document.project"), context);
        QVERIFY(dynamic_cast<MarkdownView *>(page(view.get())));

        context.itemConfig = {{QStringLiteral("folder"), QStringLiteral("f1")}, {QStringLiteral("path"), QStringLiteral("large.txt")}};
        view = make(QStringLiteral("document.project"), context);
        QCOMPARE(messageOf(view.get()), QStringLiteral("This file is too large to show."));
    }

    void pdfIsShown()
    {
        QTemporaryDir folder;
        QVERIFY(writeFile(folder.filePath(QStringLiteral("spec.pdf")), pdfBytes()));
        QVERIFY(writeFile(folder.filePath(QStringLiteral("broken.pdf")), "not a pdf"));
        FakeContext context;
        context.folders.insert(QStringLiteral("f1"), folder.path());
        context.itemConfig = {{QStringLiteral("folder"), QStringLiteral("f1")}, {QStringLiteral("path"), QStringLiteral("spec.pdf")}};
        auto view = make(QStringLiteral("document.project"), context);
        auto *pdf = qobject_cast<QPdfView *>(page(view.get()));
        QVERIFY(pdf);
        QVERIFY(pdf->document());
        QCOMPARE(pdf->document()->pageCount(), 1);

        context.itemConfig.insert(QStringLiteral("path"), QStringLiteral("broken.pdf"));
        view = make(QStringLiteral("document.project"), context);
        QCOMPARE(messageOf(view.get()), QStringLiteral("The PDF file cannot be read."));
    }

    // A workspace file is read through filePath(), and its original name
    // chooses the format.
    void workspaceFile()
    {
        QTemporaryDir files;
        const QString path = files.filePath(QStringLiteral("0a1b.txt"));
        QVERIFY(writeFile(path, "# Title"));
        FakeContext context;
        context.files.insert(QStringLiteral("0a1b.txt"), path);
        context.itemConfig = {{QStringLiteral("file"), QStringLiteral("0a1b.txt")},
                              {QStringLiteral("originalName"), QStringLiteral("notes.md")}};
        auto view = make(QStringLiteral("document.file"), context);
        QVERIFY(dynamic_cast<MarkdownView *>(page(view.get())));

        context.itemConfig.insert(QStringLiteral("file"), QStringLiteral("gone.md"));
        view = make(QStringLiteral("document.file"), context);
        QCOMPARE(messageOf(view.get()), QStringLiteral("The file is missing. It may not have synced to this device yet."));
    }

    // A download is shown; a reload sends the ETag and keeps the file on a
    // 304; a failed reload keeps the file with the reason above it.
    void downloadWithETag()
    {
        HttpServer server;
        server.replies << HttpServer::Reply{200, "text/plain; charset=utf-8", "# Remote", "\"v1\""};
        FakeContext context;
        context.itemConfig = {{QStringLiteral("url"), server.url(QStringLiteral("README.md")).toString()}};
        auto view = make(QStringLiteral("document.url"), context);
        QTRY_VERIFY(dynamic_cast<MarkdownView *>(page(view.get())));
        auto *markdown = dynamic_cast<MarkdownView *>(page(view.get()));
        QCOMPARE(markdown->markdownText(), QStringLiteral("# Remote"));

        server.replies << HttpServer::Reply{304, {}, {}, "\"v1\""};
        click(view.get(), QStringLiteral("reload"));
        QTRY_COMPARE(server.requests.size(), 2);
        QVERIFY(server.requests.at(1).toLower().contains("if-none-match: \"v1\""));
        QTest::qWait(50);
        QCOMPARE(markdown->markdownText(), QStringLiteral("# Remote"));
        QVERIFY(problemOf(view.get()).isEmpty());

        server.replies << HttpServer::Reply{500, "text/plain", "boom", {}};
        click(view.get(), QStringLiteral("reload"));
        QTRY_VERIFY(problemOf(view.get()).startsWith(QLatin1String("Could not reload: ")));
        QCOMPARE(page(view.get()), markdown);

        server.replies << HttpServer::Reply{200, "text/plain", "# Changed", "\"v2\""};
        click(view.get(), QStringLiteral("reload"));
        QTRY_COMPARE(markdown->markdownText(), QStringLiteral("# Changed"));
        QVERIFY(problemOf(view.get()).isEmpty());
    }

    void downloadFailsFirst()
    {
        HttpServer server;
        FakeContext context;
        context.itemConfig = {{QStringLiteral("url"), server.url(QStringLiteral("gone.md")).toString()}};
        auto view = make(QStringLiteral("document.url"), context);
        QTRY_VERIFY(messageOf(view.get()).startsWith(QLatin1String("The file could not be downloaded: ")));

        context.itemConfig = {{QStringLiteral("url"), QStringLiteral("not an address")}};
        view = make(QStringLiteral("document.url"), context);
        auto *label = qobject_cast<QLabel *>(view.get());
        QVERIFY(label);
        QCOMPARE(label->text(), QStringLiteral("Set the address in the item's settings."));
    }

    // The editor copies the chosen file through saveFile, keeps keys it does
    // not show, and needs a file.
    void editorCopiesTheFile()
    {
        QStringList copied;
        mirante::EditorContext context;
        context.saveFile = [&copied](const QString &path) {
            copied << path;
            return QStringLiteral("c0ffee.pdf");
        };
        mirante::modules::document::DocumentEditor editor(context, nullptr);
        editor.setConfig({{QStringLiteral("extra"), 1}});
        QCOMPARE(editor.problems(), QStringList{QStringLiteral("Choose a file.")});

        editor.setChooser([] { return QStringLiteral("/downloads/datasheet.pdf"); });
        editor.chooseFile();
        QCOMPARE(copied, QStringList{QStringLiteral("/downloads/datasheet.pdf")});
        QVERIFY(editor.problems().isEmpty());
        auto *format = editor.findChild<QComboBox *>(QStringLiteral("format"));
        format->setCurrentIndex(format->findData(QStringLiteral("pdf")));
        const QJsonObject config = editor.config();
        QCOMPARE(config.value(QStringLiteral("file")).toString(), QStringLiteral("c0ffee.pdf"));
        QCOMPARE(config.value(QStringLiteral("originalName")).toString(), QStringLiteral("datasheet.pdf"));
        QCOMPARE(config.value(QStringLiteral("format")).toString(), QStringLiteral("pdf"));
        QCOMPARE(config.value(QStringLiteral("extra")).toInt(), 1);
        QCOMPARE(editor.findChild<QLabel *>(QStringLiteral("fileName"))->text(), QStringLiteral("datasheet.pdf"));

        // A copy that fails, or a cancelled choice, changes nothing.
        context.saveFile = [](const QString &) { return QString(); };
        mirante::modules::document::DocumentEditor refused(context, nullptr);
        refused.setChooser([] { return QStringLiteral("/downloads/huge.pdf"); });
        refused.chooseFile();
        QVERIFY(!refused.config().contains(QStringLiteral("file")));
    }
};

QTEST_MAIN(TestViews)
#include "tst_views.moc"
