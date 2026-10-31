// The Links type: reading and cleaning links, its editor and its view, with
// fake contexts. Opening goes to a recorder, so no program starts. See
// README.md#links.
#include "../Links.h"
#include "../LinksEditor.h"
#include "../LinksView.h"

#include <mirante/ItemContext.h>

#include <QAction>
#include <QClipboard>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include <memory>

namespace notes = mirante::modules::notes;
using notes::linkProblem;

namespace {

// Stands in for the system's handlers of the schemes links open.
class UrlRecorder : public QObject
{
    Q_OBJECT

public:
    UrlRecorder()
    {
        for (const char *scheme : {"http", "https", "mailto", "file"})
            QDesktopServices::setUrlHandler(QString::fromLatin1(scheme), this, "record");
    }
    ~UrlRecorder() override
    {
        for (const char *scheme : {"http", "https", "mailto", "file"})
            QDesktopServices::unsetUrlHandler(QString::fromLatin1(scheme));
    }

    QList<QUrl> urls;

public slots:
    void record(const QUrl &url) { urls << url; }
};

class FakeContext : public mirante::ItemContext
{
public:
    QJsonObject itemConfig;
    QHash<QString, QString> paths;
    QHash<QString, QString> pathsAfterMap;
    QHash<QString, QString> files;
    QStringList mapped;
    QStringList shownFolders;

    QString itemName() const override { return QStringLiteral("Links"); }
    QJsonObject config() const override { return itemConfig; }
    QJsonValue projectSetting(const QString &) const override { return {}; }
    QJsonObject moduleSettings(const QString &) const override { return {}; }
    QString folderPath(const QString &folderId) const override { return paths.value(folderId); }
    void mapFolder(const QString &folderId) override
    {
        mapped << folderId;
        if (pathsAfterMap.contains(folderId))
            paths.insert(folderId, pathsAfterMap.value(folderId));
    }
    QWidget *createWebView(const QUrl &, QWidget *) override { return nullptr; }
    QWebEngineProfile *profile() override { return nullptr; }
    QWidget *createMessageView(const QString &, QWidget *) override { return nullptr; }
    void updateConfig(const QJsonObject &) override {}
    QString filePath(const QString &fileName) const override { return files.value(fileName); }
    void showFolder(const QString &path) override { shownFolders << path; }

    void setLinks(const QList<QJsonObject> &links)
    {
        QJsonArray array;
        for (const QJsonObject &link : links)
            array.append(link);
        itemConfig.insert(notes::kLinks, array);
    }
};

QJsonObject urlLink(const QString &url, const QString &title = {})
{
    return {{notes::kLinkKind, QStringLiteral("url")}, {notes::kLinkUrl, url}, {notes::kLinkTitle, title}};
}

QJsonObject projectLink(const QString &folder, const QString &path)
{
    return {{notes::kLinkKind, QStringLiteral("project")}, {notes::kLinkFolder, folder}, {notes::kLinkPath, path}};
}

QJsonObject workspaceLink(const QString &file, const QString &original = {})
{
    return {{notes::kLinkKind, QStringLiteral("workspace")},
            {notes::kLinkFile, file},
            {notes::kLinkOriginalName, original}};
}

QListWidget *listOf(QWidget *view)
{
    return view->findChild<QListWidget *>(QStringLiteral("links"));
}

QLabel *problemOf(QWidget *view)
{
    return view->findChild<QLabel *>(QStringLiteral("problem"));
}

void clickRow(QListWidget *list, int row)
{
    QTest::mouseClick(list->viewport(), Qt::LeftButton, {}, list->visualItemRect(list->item(row)).center());
}

mirante::EditorContext editorContext()
{
    mirante::EditorContext context;
    context.folders = {{QStringLiteral("f1"), QStringLiteral("Backend")}, {QStringLiteral("f2"), QStringLiteral("Docs")}};
    context.folderPath = [](const QString &id) {
        return id == QLatin1String("f1") ? QStringLiteral("C:/work/backend") : QString();
    };
    context.saveFile = [](const QString &path) {
        return path.contains(QLatin1String("refused")) ? QString() : QStringLiteral("abc.pdf");
    };
    return context;
}

template<typename T>
T *field(QWidget *editor, const char *name)
{
    return editor->findChild<T *>(QString::fromLatin1(name));
}

void click(QWidget *editor, const char *name)
{
    field<QPushButton>(editor, name)->click();
}

void type(QWidget *editor, const char *name, const QString &text)
{
    QLineEdit *edit = field<QLineEdit>(editor, name);
    edit->setText(text);
    emit edit->textEdited(text);
}

} // namespace

class TestLinks : public QObject
{
    Q_OBJECT

private slots:
    void addresses_data()
    {
        QTest::addColumn<QString>("address");
        QTest::addColumn<bool>("openable");
        QTest::newRow("https") << QStringLiteral("https://example.com/a?b=c") << true;
        QTest::newRow("http") << QStringLiteral("http://example.com") << true;
        QTest::newRow("spaces around") << QStringLiteral("  https://example.com ") << true;
        QTest::newRow("mailto") << QStringLiteral("mailto:ana@example.com") << true;
        QTest::newRow("no scheme") << QStringLiteral("example.com") << false;
        QTest::newRow("no host") << QStringLiteral("https://") << false;
        QTest::newRow("empty mailto") << QStringLiteral("mailto:") << false;
        QTest::newRow("file") << QStringLiteral("file:///C:/Windows/system32/calc.exe") << false;
        QTest::newRow("script") << QStringLiteral("javascript:alert(1)") << false;
        QTest::newRow("other program") << QStringLiteral("ms-settings:display") << false;
    }

    void addresses()
    {
        QFETCH(QString, address);
        QFETCH(bool, openable);
        QCOMPARE(notes::isOpenableAddress(address), openable);
    }

    void paths_data()
    {
        QTest::addColumn<QString>("path");
        QTest::addColumn<bool>("inside");
        QTest::newRow("empty") << QString() << true;
        QTest::newRow("file") << QStringLiteral("docs/a.md") << true;
        QTest::newRow("backslashes") << QStringLiteral("docs\\a.md") << true;
        QTest::newRow("dots in a name") << QStringLiteral("a..b/c..md") << true;
        QTest::newRow("parent") << QStringLiteral("../secret") << false;
        QTest::newRow("parent with backslashes") << QStringLiteral("docs\\..\\..\\secret") << false;
        QTest::newRow("parent in the middle") << QStringLiteral("docs/../../secret") << false;
        QTest::newRow("absolute") << QStringLiteral("/etc/passwd") << false;
        QTest::newRow("windows absolute") << QStringLiteral("C:/Windows") << false;
        QTest::newRow("relative to a drive") << QStringLiteral("C:Windows") << false;
        QTest::newRow("network") << QStringLiteral("\\\\server\\share") << false;
    }

    void paths()
    {
        QFETCH(QString, path);
        QFETCH(bool, inside);
        QCOMPARE(notes::isInsideFolder(path), inside);
    }

    void titles()
    {
        QCOMPARE(notes::linkTitle(urlLink(QStringLiteral("https://a.example"), QStringLiteral("  A  "))),
                 QStringLiteral("A"));
        QCOMPARE(notes::linkTitle(urlLink(QStringLiteral("https://a.example"))), QStringLiteral("https://a.example"));
        QCOMPARE(notes::linkTitle(projectLink(QStringLiteral("f1"), QStringLiteral("docs\\a.md"))),
                 QStringLiteral("a.md"));
        QCOMPARE(notes::linkTitle(projectLink(QStringLiteral("f1"), QString())), QStringLiteral("Project folder"));
        QCOMPARE(notes::linkTitle(workspaceLink(QStringLiteral("abc.pdf"), QStringLiteral("spec.pdf"))),
                 QStringLiteral("spec.pdf"));
        QCOMPARE(notes::linkTitle(workspaceLink(QStringLiteral("abc.pdf"))), QStringLiteral("abc.pdf"));
    }

    void problems()
    {
        QVERIFY(linkProblem(urlLink(QStringLiteral("https://a.example"))).isEmpty());
        QVERIFY(!linkProblem(urlLink(QString())).isEmpty());
        QVERIFY(!linkProblem(urlLink(QStringLiteral("example.com"))).isEmpty());
        QVERIFY(linkProblem(projectLink(QStringLiteral("f1"), QString())).isEmpty());
        QVERIFY(!linkProblem(projectLink(QString(), QStringLiteral("a.md"))).isEmpty());
        QVERIFY(!linkProblem(projectLink(QStringLiteral("f1"), QStringLiteral("../a.md"))).isEmpty());
        QVERIFY(linkProblem(workspaceLink(QStringLiteral("abc.pdf"))).isEmpty());
        QVERIFY(!linkProblem(workspaceLink(QString())).isEmpty());
    }

    // The keys of other kinds go, so that a file link changed into an address
    // no longer refers to its file, and files/ can drop it. Unknown keys stay.
    void cleaning()
    {
        QJsonObject link = urlLink(QStringLiteral(" https://a.example "), QStringLiteral(" "));
        link.insert(notes::kLinkFile, QStringLiteral("abc.pdf"));
        link.insert(notes::kLinkPath, QStringLiteral("a.md"));
        link.insert(QStringLiteral("fromNewerVersion"), 1);
        const QJsonObject cleaned = notes::cleanedLink(link);
        QCOMPARE(cleaned.value(notes::kLinkUrl).toString(), QStringLiteral("https://a.example"));
        QVERIFY(!cleaned.contains(notes::kLinkTitle));
        QVERIFY(!cleaned.contains(notes::kLinkFile));
        QVERIFY(!cleaned.contains(notes::kLinkPath));
        QCOMPARE(cleaned.value(QStringLiteral("fromNewerVersion")).toInt(), 1);

        const QJsonObject project = notes::cleanedLink(projectLink(QStringLiteral("f1"), QStringLiteral("docs\\a.md")));
        QCOMPARE(project.value(notes::kLinkPath).toString(), QStringLiteral("docs/a.md"));
    }

    void readsOnlyObjects()
    {
        QJsonObject config;
        config.insert(notes::kLinks, QJsonArray{QStringLiteral("not a link"), urlLink(QStringLiteral("https://a.example"))});
        QCOMPARE(notes::linksOf(config).size(), 1);
        QVERIFY(notes::linksOf(QJsonObject()).isEmpty());
    }

    void editorAddsAndReorders()
    {
        notes::LinksEditor editor(editorContext(), nullptr);
        QJsonObject start{{QStringLiteral("other"), 1}};
        editor.setConfig(start);
        QVERIFY(!field<QWidget>(&editor, "title")->parentWidget()->isEnabled());

        click(&editor, "addLink");
        type(&editor, "title", QStringLiteral("First"));
        type(&editor, "description", QStringLiteral("Short"));
        type(&editor, "url", QStringLiteral("https://one.example"));
        click(&editor, "addLink");
        type(&editor, "url", QStringLiteral("https://two.example"));

        auto *list = field<QListWidget>(&editor, "linkList");
        QCOMPARE(list->count(), 2);
        QCOMPARE(list->item(0)->text(), QStringLiteral("First"));
        QCOMPARE(list->item(1)->text(), QStringLiteral("https://two.example"));
        QVERIFY(editor.problems().isEmpty());

        click(&editor, "moveUp");
        QJsonObject config = editor.config();
        QCOMPARE(config.value(QStringLiteral("other")).toInt(), 1);
        QJsonArray links = config.value(notes::kLinks).toArray();
        QCOMPARE(links.size(), 2);
        QCOMPARE(links.at(0).toObject().value(notes::kLinkUrl).toString(), QStringLiteral("https://two.example"));
        QCOMPARE(links.at(1).toObject().value(notes::kLinkDescription).toString(), QStringLiteral("Short"));
        QVERIFY(!field<QPushButton>(&editor, "moveUp")->isEnabled());

        click(&editor, "removeLink");
        click(&editor, "removeLink");
        QCOMPARE(list->count(), 0);
        QVERIFY(!editor.config().contains(notes::kLinks));
    }

    void editorReportsProblems()
    {
        notes::LinksEditor editor(editorContext(), nullptr);
        editor.setConfig({});
        click(&editor, "addLink");
        QCOMPARE(editor.problems(), QStringList{QStringLiteral("Link 1: Enter the address.")});
        type(&editor, "url", QStringLiteral("example.com"));
        QCOMPARE(editor.problems().size(), 1);
        type(&editor, "url", QStringLiteral("https://example.com"));
        QVERIFY(editor.problems().isEmpty());
    }

    void editorMakesProjectLinks()
    {
        notes::LinksEditor editor(editorContext(), nullptr);
        editor.setConfig({});
        editor.setProjectChooser([](const QString &root) {
            return root + QStringLiteral("/docs/a.md");
        });
        click(&editor, "addLink");
        type(&editor, "url", QStringLiteral("https://dropped.example"));
        auto *kind = field<QComboBox>(&editor, "kind");
        kind->setCurrentIndex(kind->findData(QStringLiteral("project")));

        QVERIFY(field<QPushButton>(&editor, "browse")->isEnabled());
        click(&editor, "browse");
        QCOMPARE(field<QLineEdit>(&editor, "path")->text(), QStringLiteral("docs/a.md"));

        const QJsonObject link = editor.config().value(notes::kLinks).toArray().at(0).toObject();
        QCOMPARE(link.value(notes::kLinkKind).toString(), QStringLiteral("project"));
        QCOMPARE(link.value(notes::kLinkFolder).toString(), QStringLiteral("f1"));
        QCOMPARE(link.value(notes::kLinkPath).toString(), QStringLiteral("docs/a.md"));
        QVERIFY(!link.contains(notes::kLinkUrl));

        // A file outside the folder is refused.
        editor.setProjectChooser([](const QString &) { return QStringLiteral("D:/elsewhere/b.md"); });
        click(&editor, "browse");
        QCOMPARE(field<QLineEdit>(&editor, "path")->text(), QStringLiteral("docs/a.md"));
        QVERIFY(!field<QLabel>(&editor, "pathNote")->text().isEmpty());

        // A folder without a local path cannot be browsed, and says so.
        auto *folder = field<QComboBox>(&editor, "folder");
        folder->setCurrentIndex(folder->findData(QStringLiteral("f2")));
        QVERIFY(!field<QPushButton>(&editor, "browse")->isEnabled());
        QCOMPARE(field<QLabel>(&editor, "pathNote")->text(), QStringLiteral("Folder not set on this device"));
    }

    void editorKeepsAFolderThatLeftTheProject()
    {
        notes::LinksEditor editor(editorContext(), nullptr);
        QJsonObject config;
        config.insert(notes::kLinks, QJsonArray{projectLink(QStringLiteral("gone"), QStringLiteral("a.md"))});
        editor.setConfig(config);
        QCOMPARE(field<QComboBox>(&editor, "folder")->currentData().toString(), QStringLiteral("gone"));
        const QJsonObject link = editor.config().value(notes::kLinks).toArray().at(0).toObject();
        QCOMPARE(link.value(notes::kLinkFolder).toString(), QStringLiteral("gone"));
    }

    void editorCopiesWorkspaceFiles()
    {
        notes::LinksEditor editor(editorContext(), nullptr);
        editor.setConfig({});
        click(&editor, "addLink");
        auto *kind = field<QComboBox>(&editor, "kind");
        kind->setCurrentIndex(kind->findData(QStringLiteral("workspace")));
        QCOMPARE(editor.problems().size(), 1);

        // A refused copy changes nothing.
        editor.setFileChooser([] { return QStringLiteral("/downloads/refused.pdf"); });
        click(&editor, "chooseFile");
        QCOMPARE(editor.problems().size(), 1);

        editor.setFileChooser([] { return QStringLiteral("/downloads/spec.pdf"); });
        click(&editor, "chooseFile");
        QVERIFY(editor.problems().isEmpty());
        QCOMPARE(field<QLabel>(&editor, "fileName")->text(), QStringLiteral("spec.pdf (a copy in the workspace)"));
        QCOMPARE(field<QListWidget>(&editor, "linkList")->item(0)->text(), QStringLiteral("spec.pdf"));
        const QJsonObject link = editor.config().value(notes::kLinks).toArray().at(0).toObject();
        QCOMPARE(link.value(notes::kLinkFile).toString(), QStringLiteral("abc.pdf"));
        QCOMPARE(link.value(notes::kLinkOriginalName).toString(), QStringLiteral("spec.pdf"));

        // Changing the kind drops the reference, so the file can be deleted.
        kind->setCurrentIndex(kind->findData(QStringLiteral("url")));
        QVERIFY(!editor.config().value(notes::kLinks).toArray().at(0).toObject().contains(notes::kLinkFile));
    }

    void viewShowsTitlesAndDescriptions()
    {
        FakeContext context;
        QJsonObject link = urlLink(QStringLiteral("https://a.example"), QStringLiteral("A"));
        link.insert(notes::kLinkDescription, QStringLiteral("Short"));
        context.setLinks({link, urlLink(QStringLiteral("https://b.example"))});
        std::unique_ptr<QWidget> view(notes::createLinksView(context, nullptr));

        QListWidget *list = listOf(view.get());
        QCOMPARE(list->count(), 2);
        QCOMPARE(list->item(0)->text(), QStringLiteral("A"));
        QCOMPARE(list->item(0)->data(Qt::UserRole).toString(), QStringLiteral("Short"));
        QCOMPARE(list->item(0)->toolTip(), QStringLiteral("https://a.example"));
        QCOMPARE(list->item(1)->text(), QStringLiteral("https://b.example"));
        // A description makes the row taller.
        view->show();
        QVERIFY(list->visualItemRect(list->item(0)).height() > list->visualItemRect(list->item(1)).height());
    }

    void clickOpensAddress()
    {
        UrlRecorder recorder;
        FakeContext context;
        context.setLinks({urlLink(QStringLiteral("https://a.example/x"))});
        std::unique_ptr<QWidget> view(notes::createLinksView(context, nullptr));
        view->show();
        clickRow(listOf(view.get()), 0);
        QCOMPARE(recorder.urls, QList<QUrl>{QUrl(QStringLiteral("https://a.example/x"))});
    }

    void returnOpensCurrentRow()
    {
        UrlRecorder recorder;
        FakeContext context;
        context.setLinks({urlLink(QStringLiteral("https://a.example")), urlLink(QStringLiteral("https://b.example"))});
        std::unique_ptr<QWidget> view(notes::createLinksView(context, nullptr));
        QListWidget *list = listOf(view.get());
        list->setCurrentRow(1);
        QTest::keyClick(list, Qt::Key_Return);
        QCOMPARE(recorder.urls, QList<QUrl>{QUrl(QStringLiteral("https://b.example"))});
    }

    // Stored data syncs, so the view refuses what the editor would refuse.
    void refusedAddressesAndPathsDoNotOpen()
    {
        UrlRecorder recorder;
        FakeContext context;
        context.paths.insert(QStringLiteral("f1"), QDir::tempPath());
        context.setLinks({urlLink(QStringLiteral("file:///C:/Windows/system32/calc.exe")),
                          projectLink(QStringLiteral("f1"), QStringLiteral("../secret"))});
        std::unique_ptr<QWidget> view(notes::createLinksView(context, nullptr));
        view->show();
        QListWidget *list = listOf(view.get());
        clickRow(list, 0);
        QVERIFY(recorder.urls.isEmpty());
        QVERIFY(!problemOf(view.get())->isHidden());
        clickRow(list, 1);
        QVERIFY(recorder.urls.isEmpty());
        QVERIFY(!problemOf(view.get())->isHidden());
    }

    void opensProjectFile()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("docs")));
        QFile file(dir.filePath(QStringLiteral("docs/a.md")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();

        UrlRecorder recorder;
        FakeContext context;
        context.paths.insert(QStringLiteral("f1"), dir.path());
        context.setLinks({projectLink(QStringLiteral("f1"), QStringLiteral("docs/a.md")),
                          projectLink(QStringLiteral("f1"), QStringLiteral("docs/gone.md")),
                          projectLink(QStringLiteral("f1"), QString())});
        std::unique_ptr<QWidget> view(notes::createLinksView(context, nullptr));
        view->show();
        QListWidget *list = listOf(view.get());

        clickRow(list, 0);
        QCOMPARE(recorder.urls, QList<QUrl>{QUrl::fromLocalFile(file.fileName())});
        QVERIFY(problemOf(view.get())->isHidden());

        clickRow(list, 1);
        QCOMPARE(recorder.urls.size(), 1);
        QCOMPARE(problemOf(view.get())->text(), QStringLiteral("The file is missing: gone.md"));

        // A later click clears the message. An empty path is the folder.
        clickRow(list, 2);
        QVERIFY(problemOf(view.get())->isHidden());
        QCOMPARE(recorder.urls.last(), QUrl::fromLocalFile(dir.path()));
    }

    void unmappedFolderAsksForItsPath()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        UrlRecorder recorder;
        FakeContext context;
        context.setLinks({projectLink(QStringLiteral("f1"), QString())});
        std::unique_ptr<QWidget> view(notes::createLinksView(context, nullptr));
        view->show();

        // Cancelled: nothing opens and nothing is reported.
        clickRow(listOf(view.get()), 0);
        QCOMPARE(context.mapped, QStringList{QStringLiteral("f1")});
        QVERIFY(recorder.urls.isEmpty());
        QVERIFY(problemOf(view.get())->isHidden());

        context.pathsAfterMap.insert(QStringLiteral("f1"), dir.path());
        clickRow(listOf(view.get()), 0);
        QCOMPARE(recorder.urls, QList<QUrl>{QUrl::fromLocalFile(dir.path())});
    }

    void opensWorkspaceFile()
    {
        UrlRecorder recorder;
        FakeContext context;
        context.files.insert(QStringLiteral("abc.pdf"), QStringLiteral("C:/data/files/abc.pdf"));
        context.setLinks({workspaceLink(QStringLiteral("abc.pdf")), workspaceLink(QStringLiteral("gone.pdf"))});
        std::unique_ptr<QWidget> view(notes::createLinksView(context, nullptr));
        view->show();
        clickRow(listOf(view.get()), 0);
        QCOMPARE(recorder.urls, QList<QUrl>{QUrl::fromLocalFile(QStringLiteral("C:/data/files/abc.pdf"))});
        clickRow(listOf(view.get()), 1);
        QCOMPARE(recorder.urls.size(), 1);
        QVERIFY(problemOf(view.get())->text().contains(QStringLiteral("synced")));
    }

    void menuActions()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile file(dir.filePath(QStringLiteral("a.md")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();

        FakeContext context;
        context.paths.insert(QStringLiteral("f1"), dir.path());
        context.setLinks({urlLink(QStringLiteral("https://a.example")),
                          projectLink(QStringLiteral("f1"), QStringLiteral("a.md"))});
        std::unique_ptr<QWidget> view(notes::createLinksView(context, nullptr));
        QListWidget *list = listOf(view.get());
        auto *copy = list->findChild<QAction *>(QStringLiteral("copyLink"));
        auto *show = list->findChild<QAction *>(QStringLiteral("showLink"));

        list->setCurrentRow(0);
        QVERIFY(!show->isEnabled());
        copy->trigger();
        QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("https://a.example"));

        list->setCurrentRow(1);
        QVERIFY(show->isEnabled());
        copy->trigger();
        QCOMPARE(QGuiApplication::clipboard()->text(), QDir::toNativeSeparators(file.fileName()));
        show->trigger();
        QCOMPARE(context.shownFolders, QStringList{dir.path()});
    }
};

QTEST_MAIN(TestLinks)
#include "tst_links.moc"
