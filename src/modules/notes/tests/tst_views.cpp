// The Notes view, with a fake context that keeps the configuration and calls
// the view back as the core does. See README.md#view.
#include "../NotesView.h"

#include <mirante/ItemContext.h>

#include <QApplication>
#include <QFontDatabase>
#include <QImage>
#include <QJsonObject>
#include <QPlainTextEdit>
#include <QPointer>
#include <QStackedWidget>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QTest>
#include <QTextBrowser>
#include <QTextDocument>
#include <QVBoxLayout>

#include <functional>
#include <memory>

namespace notes = mirante::modules::notes;

namespace {

const QString kText = QStringLiteral("text");

class FakeContext : public mirante::ItemContext
{
public:
    QJsonObject itemConfig;
    QList<QJsonObject> saved;
    QPointer<QObject> receiver;
    std::function<void(const QJsonObject &)> changed;

    QString itemName() const override { return QStringLiteral("Notes"); }
    QJsonObject config() const override { return itemConfig; }
    QJsonValue projectSetting(const QString &) const override { return {}; }
    QJsonObject moduleSettings(const QString &) const override { return {}; }
    QString folderPath(const QString &) const override { return {}; }
    QWidget *createWebView(const QUrl &, QWidget *) override { return nullptr; }
    QWebEngineProfile *profile() override { return nullptr; }
    QWidget *createMessageView(const QString &, QWidget *) override { return nullptr; }

    // Stores the change and tells the view, as a placement does for a change
    // of view keys only.
    void updateConfig(const QJsonObject &config) override
    {
        saved << config;
        itemConfig = config;
        notify();
    }

    void onConfigChanged(QObject *object, std::function<void(const QJsonObject &)> callback) override
    {
        receiver = object;
        changed = std::move(callback);
    }

    // A change made on another device.
    void changeElsewhere(const QJsonObject &config)
    {
        itemConfig = config;
        notify();
    }

private:
    void notify()
    {
        if (receiver && changed)
            changed(itemConfig);
    }
};

QPlainTextEdit *editorOf(QWidget *view) { return view->findChild<QPlainTextEdit *>(); }
QTextBrowser *previewOf(QWidget *view) { return view->findChild<QTextBrowser *>(); }
bool inPreview(QWidget *view) { return view->findChild<QStackedWidget *>()->currentWidget() == previewOf(view); }

// Types text at the end of the editor, as the user does.
void type(QPlainTextEdit *editor, const QString &text)
{
    QTextCursor cursor = editor->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(text);
}

} // namespace

class TestViews : public QObject
{
    Q_OBJECT

private slots:
    void opensInPreviewWithText()
    {
        FakeContext context;
        context.itemConfig = {{kText, QStringLiteral("# Title")}};
        std::unique_ptr<QWidget> view(notes::createNotesView(context, nullptr));
        QVERIFY(inPreview(view.get()));
        QCOMPARE(editorOf(view.get())->toPlainText(), QStringLiteral("# Title"));
        QVERIFY(previewOf(view.get())->toPlainText().contains(QStringLiteral("Title")));
        QVERIFY(!previewOf(view.get())->toPlainText().contains(QLatin1Char('#')));
    }

    void opensInEditWithoutText()
    {
        FakeContext context;
        std::unique_ptr<QWidget> view(notes::createNotesView(context, nullptr));
        QVERIFY(!inPreview(view.get()));
        QCOMPARE(editorOf(view.get())->font().family(), QFontDatabase::systemFont(QFontDatabase::FixedFont).family());
    }

    void savesAfterThePause()
    {
        FakeContext context;
        context.itemConfig = {{QStringLiteral("other"), 7}};
        std::unique_ptr<QWidget> view(notes::createNotesView(context, nullptr));
        type(editorOf(view.get()), QStringLiteral("a"));
        type(editorOf(view.get()), QStringLiteral("b"));
        QTest::qWait(300);
        QVERIFY(context.saved.isEmpty());

        QTRY_COMPARE(context.saved.size(), 1);
        const QJsonObject expected{{QStringLiteral("other"), 7}, {kText, QStringLiteral("ab")}};
        QCOMPARE(context.saved.first(), expected);

        // Nothing more without a change.
        QTest::qWait(1000);
        QCOMPARE(context.saved.size(), 1);
    }

    void savesWhenDestroyed()
    {
        FakeContext context;
        std::unique_ptr<QWidget> view(notes::createNotesView(context, nullptr));
        type(editorOf(view.get()), QStringLiteral("unsaved"));
        QVERIFY(context.saved.isEmpty());
        view.reset();
        QCOMPARE(context.saved.size(), 1);
        QCOMPARE(context.saved.first().value(kText).toString(), QStringLiteral("unsaved"));
    }

    void destroyedWithoutChangesSavesNothing()
    {
        FakeContext context;
        context.itemConfig = {{kText, QStringLiteral("kept")}};
        std::unique_ptr<QWidget> view(notes::createNotesView(context, nullptr));
        view.reset();
        QVERIFY(context.saved.isEmpty());
    }

    void savesWhenTheEditorLosesFocus()
    {
        FakeContext context;
        QWidget window;
        auto *layout = new QVBoxLayout(&window);
        QWidget *view = notes::createNotesView(context, &window);
        auto *other = new QPlainTextEdit(&window);
        layout->addWidget(view);
        layout->addWidget(other);
        window.show();
        window.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&window));

        editorOf(view)->setFocus();
        QTRY_VERIFY(editorOf(view)->hasFocus());
        type(editorOf(view), QStringLiteral("focus"));
        other->setFocus();
        QTRY_VERIFY(other->hasFocus());
        QCOMPARE(context.saved.size(), 1);
        QCOMPARE(context.saved.first().value(kText).toString(), QStringLiteral("focus"));
    }

    void emptyTextRemovesTheKey()
    {
        FakeContext context;
        context.itemConfig = {{kText, QStringLiteral("gone")}, {QStringLiteral("other"), true}};
        std::unique_ptr<QWidget> view(notes::createNotesView(context, nullptr));
        editorOf(view.get())->clear();
        QTRY_COMPARE(context.saved.size(), 1);
        const QJsonObject expected{{QStringLiteral("other"), true}};
        QCOMPARE(context.saved.first(), expected);
    }

    void externalChangeReplacesTheText()
    {
        FakeContext context;
        context.itemConfig = {{kText, QStringLiteral("old")}};
        std::unique_ptr<QWidget> view(notes::createNotesView(context, nullptr));
        QVERIFY(inPreview(view.get()));

        context.changeElsewhere({{kText, QStringLiteral("new **bold**")}});
        QCOMPARE(editorOf(view.get())->toPlainText(), QStringLiteral("new **bold**"));
        QVERIFY(inPreview(view.get()));
        QCOMPARE(previewOf(view.get())->toPlainText().trimmed(), QStringLiteral("new bold"));

        // Taking the change is not an edit, so nothing is written back.
        QTest::qWait(1000);
        QVERIFY(context.saved.isEmpty());
    }

    void externalChangeKeepsUnsavedEdits()
    {
        FakeContext context;
        context.itemConfig = {{kText, QStringLiteral("mine")}};
        std::unique_ptr<QWidget> view(notes::createNotesView(context, nullptr));
        type(editorOf(view.get()), QStringLiteral(" and more"));

        context.changeElsewhere({{kText, QStringLiteral("theirs")}, {QStringLiteral("other"), 1}});
        QCOMPARE(editorOf(view.get())->toPlainText(), QStringLiteral("mine and more"));

        // The local text wins when saved, over the other keys now stored.
        QTRY_COMPARE(context.saved.size(), 1);
        const QJsonObject expected{{kText, QStringLiteral("mine and more")}, {QStringLiteral("other"), 1}};
        QCOMPARE(context.saved.first(), expected);
    }

    // Neither a remote image nor a local file is read by the preview.
    void previewLoadsNoResources()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        const QString remote = QStringLiteral("http://127.0.0.1:%1/tracker.png").arg(server.serverPort());

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString localPath = dir.filePath(QStringLiteral("local.png"));
        QImage local(40, 30, QImage::Format_RGB32);
        local.fill(Qt::red);
        QVERIFY(local.save(localPath));
        const QString localUrl = QUrl::fromLocalFile(localPath).toString();

        FakeContext context;
        context.itemConfig = {{kText, QStringLiteral("![a](%1)\n\n![b](%2)\n\n![c](%3)\n")
                                          .arg(remote, localUrl, localPath)}};
        std::unique_ptr<QWidget> view(notes::createNotesView(context, nullptr));
        view->resize(400, 300);
        view->show();
        QTextBrowser *preview = previewOf(view.get());
        QVERIFY(inPreview(view.get()));
        QVERIFY(QTest::qWaitForWindowExposed(view.get()));
        preview->grab();

        for (const QString &name : {remote, localUrl, localPath}) {
            const QImage loaded = preview->document()->resource(QTextDocument::ImageResource, QUrl(name)).value<QImage>();
            QVERIFY2(loaded.size() == QSize(1, 1), qPrintable(name));
        }
        QTest::qWait(200);
        QVERIFY(!server.hasPendingConnections());
    }

    void ctrlESwitchesOnlyTheFocusedNote()
    {
        FakeContext first;
        FakeContext second;
        QWidget window;
        auto *layout = new QVBoxLayout(&window);
        QWidget *one = notes::createNotesView(first, &window);
        QWidget *two = notes::createNotesView(second, &window);
        layout->addWidget(one);
        layout->addWidget(two);
        window.show();
        window.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&window));
        QVERIFY(!inPreview(one));
        QVERIFY(!inPreview(two));

        editorOf(one)->setFocus();
        QTRY_VERIFY(editorOf(one)->hasFocus());
        QTest::keyClick(window.windowHandle(), Qt::Key_E, Qt::ControlModifier);
        QVERIFY(inPreview(one));
        QVERIFY(!inPreview(two));
        QVERIFY(previewOf(one)->hasFocus());

        QTest::keyClick(window.windowHandle(), Qt::Key_E, Qt::ControlModifier);
        QVERIFY(!inPreview(one));
        QVERIFY(editorOf(one)->hasFocus());
        QVERIFY(!inPreview(two));
    }
};

QTEST_MAIN(TestViews)
#include "tst_views.moc"
