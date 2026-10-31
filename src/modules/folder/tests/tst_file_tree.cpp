// The File tree view, on a temporary folder, with a fake context that records
// the programs it would start. Opening goes to a recorder. See README.md#file-tree.
#include "../FileTreeView.h"

#include <mirante/ItemContext.h>

#include <QAbstractItemModel>
#include <QAction>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHash>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeView>
#include <QUrl>

#include <memory>

namespace folder = mirante::modules::folder;

namespace {

class UrlRecorder : public QObject
{
    Q_OBJECT

public:
    UrlRecorder() { QDesktopServices::setUrlHandler(QStringLiteral("file"), this, "record"); }
    ~UrlRecorder() override { QDesktopServices::unsetUrlHandler(QStringLiteral("file")); }

    QList<QUrl> urls;

public slots:
    void record(const QUrl &url) { urls << url; }
};

class FakeContext : public mirante::ItemContext
{
public:
    QJsonObject itemConfig{{QStringLiteral("folder"), QStringLiteral("f1")}};
    QHash<QString, QString> paths;
    QHash<QString, QString> pathsAfterMap;
    QStringList mapped;
    QStringList shownFolders;
    QStringList terminals;
    QString programDir;
    QString program;
    QStringList programArguments;
    QString error;

    QString itemName() const override { return QStringLiteral("Files"); }
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
    QJsonValue deviceSetting(const QString &key) const override
    {
        return key == QLatin1String("editor") ? QJsonValue(QStringLiteral("code -n")) : QJsonValue();
    }
    QString launchTerminal(const QString &dir, const QString &, const QStringList &) override
    {
        terminals << dir;
        return error;
    }
    QString startProgram(const QString &dir, const QString &name, const QStringList &arguments) override
    {
        programDir = dir;
        program = name;
        programArguments = arguments;
        return error;
    }
    void showFolder(const QString &path) override { shownFolders << path; }
};

void writeFile(const QString &path)
{
    QFile file(path);
    QVERIFY2(file.open(QIODevice::WriteOnly), qPrintable(path));
}

// A project folder: src/main.cpp, a .git folder, and files that sort after
// the folders by name but before them by case.
void fill(const QTemporaryDir &dir)
{
    QDir root(dir.path());
    QVERIFY(root.mkpath(QStringLiteral("src")));
    QVERIFY(root.mkpath(QStringLiteral("zdocs")));
    QVERIFY(root.mkpath(QStringLiteral(".git/objects")));
    writeFile(dir.filePath(QStringLiteral("src/main.cpp")));
    writeFile(dir.filePath(QStringLiteral("zdocs/a.md")));
    writeFile(dir.filePath(QStringLiteral(".git/HEAD")));
    writeFile(dir.filePath(QStringLiteral("Apple.txt")));
    writeFile(dir.filePath(QStringLiteral("banana.txt")));
}

QTreeView *treeOf(QWidget *view)
{
    return view->findChild<QTreeView *>(QStringLiteral("tree"));
}

QStringList namesUnder(QTreeView *tree, const QModelIndex &parent)
{
    QStringList names;
    const QAbstractItemModel *model = tree->model();
    for (int row = 0; row < model->rowCount(parent); ++row)
        names << model->index(row, 0, parent).data().toString();
    return names;
}

QModelIndex childNamed(QTreeView *tree, const QModelIndex &parent, const QString &name)
{
    const QAbstractItemModel *model = tree->model();
    for (int row = 0; row < model->rowCount(parent); ++row) {
        const QModelIndex index = model->index(row, 0, parent);
        if (index.data().toString() == name)
            return index;
    }
    return {};
}

void selectRow(QTreeView *tree, const QModelIndex &index)
{
    tree->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}

void trigger(QTreeView *tree, const QString &name)
{
    auto *action = tree->findChild<QAction *>(name);
    QVERIFY2(action, qPrintable(name));
    action->trigger();
}

} // namespace

class TestFileTree : public QObject
{
    Q_OBJECT

private slots:
    // Folders come first, by name without regard to case. .git never shows.
    void showsSortedFilesWithoutGit()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        fill(dir);
        FakeContext context;
        context.paths.insert(QStringLiteral("f1"), dir.path());
        std::unique_ptr<QWidget> view(folder::createFileTreeView(context, nullptr));
        view->show();

        QTreeView *tree = treeOf(view.get());
        const QStringList expected{QStringLiteral("src"), QStringLiteral("zdocs"), QStringLiteral("Apple.txt"),
                                   QStringLiteral("banana.txt")};
        QTRY_COMPARE(namesUnder(tree, tree->rootIndex()), expected);
        QVERIFY(tree->isHeaderHidden());
        for (int column = 1; column < tree->model()->columnCount(tree->rootIndex()); ++column)
            QVERIFY(tree->isColumnHidden(column));
    }

    void noModelBeforeShown()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        FakeContext context;
        context.paths.insert(QStringLiteral("f1"), dir.path());
        std::unique_ptr<QWidget> view(folder::createFileTreeView(context, nullptr));
        QVERIFY(!treeOf(view.get())->model());
        view->show();
        QVERIFY(treeOf(view.get())->model());
    }

    void expandsAFolder()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        fill(dir);
        FakeContext context;
        context.paths.insert(QStringLiteral("f1"), dir.path());
        std::unique_ptr<QWidget> view(folder::createFileTreeView(context, nullptr));
        view->show();
        QTreeView *tree = treeOf(view.get());
        QTRY_VERIFY(childNamed(tree, tree->rootIndex(), QStringLiteral("src")).isValid());

        // Found again on each try: reading the folder can sort its rows, which
        // makes an index kept from before invalid.
        const auto src = [tree] { return childNamed(tree, tree->rootIndex(), QStringLiteral("src")); };
        tree->expand(src());
        QTRY_COMPARE(namesUnder(tree, src()), QStringList{QStringLiteral("main.cpp")});
    }

    void followsChangesOnDisk()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        fill(dir);
        FakeContext context;
        context.paths.insert(QStringLiteral("f1"), dir.path());
        std::unique_ptr<QWidget> view(folder::createFileTreeView(context, nullptr));
        view->show();
        QTreeView *tree = treeOf(view.get());
        QTRY_VERIFY(childNamed(tree, tree->rootIndex(), QStringLiteral("banana.txt")).isValid());

        writeFile(dir.filePath(QStringLiteral("cherry.txt")));
        QTRY_VERIFY_WITH_TIMEOUT(childNamed(tree, tree->rootIndex(), QStringLiteral("cherry.txt")).isValid(), 10000);
    }

    void subfolderIsTheRoot()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        fill(dir);
        FakeContext context;
        context.paths.insert(QStringLiteral("f1"), dir.path());
        context.itemConfig.insert(QStringLiteral("path"), QStringLiteral("src"));
        std::unique_ptr<QWidget> view(folder::createFileTreeView(context, nullptr));
        view->show();
        QTreeView *tree = treeOf(view.get());
        QTRY_COMPARE(namesUnder(tree, tree->rootIndex()), QStringList{QStringLiteral("main.cpp")});

        // The relative path is from the project folder, not from the subfolder.
        selectRow(tree, childNamed(tree, tree->rootIndex(), QStringLiteral("main.cpp")));
        trigger(tree, QStringLiteral("copyRelativePath"));
        QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("src/main.cpp"));
    }

    void refusesPathsOutsideTheFolder_data()
    {
        QTest::addColumn<QString>("path");
        QTest::newRow("parent") << QStringLiteral("../other");
        QTest::newRow("backslashes") << QStringLiteral("src\\..\\..\\other");
        QTest::newRow("absolute") << QStringLiteral("/etc");
        QTest::newRow("windows absolute") << QStringLiteral("C:/Windows");
    }

    void refusesPathsOutsideTheFolder()
    {
        QFETCH(QString, path);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        FakeContext context;
        context.paths.insert(QStringLiteral("f1"), dir.path());
        context.itemConfig.insert(QStringLiteral("path"), path);
        std::unique_ptr<QWidget> view(folder::createFileTreeView(context, nullptr));
        view->show();
        QVERIFY(!treeOf(view.get())->model());
        QVERIFY(treeOf(view.get())->isHidden());
        QVERIFY(view->findChild<QLabel *>(QStringLiteral("message"))->text().contains(QStringLiteral("inside")));
    }

    void missingSubfolderSaysSo()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        FakeContext context;
        context.paths.insert(QStringLiteral("f1"), dir.path());
        context.itemConfig.insert(QStringLiteral("path"), QStringLiteral("gone"));
        std::unique_ptr<QWidget> view(folder::createFileTreeView(context, nullptr));
        view->show();
        QVERIFY(treeOf(view.get())->isHidden());
        QVERIFY(view->findChild<QLabel *>(QStringLiteral("message"))->text().contains(QStringLiteral("missing")));

        // It shows once the folder is there and the view shows again.
        QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("gone")));
        view->hide();
        view->show();
        QVERIFY(!treeOf(view.get())->isHidden());
        QVERIFY(treeOf(view.get())->model());
    }

    void unmappedFolderChoosesOne()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        fill(dir);
        FakeContext context;
        context.pathsAfterMap.insert(QStringLiteral("f1"), dir.path());
        std::unique_ptr<QWidget> view(folder::createFileTreeView(context, nullptr));
        view->show();

        QCOMPARE(view->findChild<QLabel *>(QStringLiteral("message"))->text(),
                 QStringLiteral("Folder not set on this device"));
        QTreeView *tree = treeOf(view.get());
        QVERIFY(tree->isHidden());
        auto *choose = view->findChild<QPushButton *>(QStringLiteral("chooseFolder"));
        QVERIFY(!choose->isHidden());

        choose->click();
        QCOMPARE(context.mapped, QStringList{QStringLiteral("f1")});
        QVERIFY(choose->isHidden());
        QVERIFY(!tree->isHidden());
        QTRY_VERIFY(childNamed(tree, tree->rootIndex(), QStringLiteral("src")).isValid());
    }

    void opensFilesAndNotFolders()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        fill(dir);
        UrlRecorder recorder;
        FakeContext context;
        context.paths.insert(QStringLiteral("f1"), dir.path());
        std::unique_ptr<QWidget> view(folder::createFileTreeView(context, nullptr));
        view->show();
        QTreeView *tree = treeOf(view.get());
        QTRY_VERIFY(childNamed(tree, tree->rootIndex(), QStringLiteral("banana.txt")).isValid());

        emit tree->activated(childNamed(tree, tree->rootIndex(), QStringLiteral("src")));
        QVERIFY(recorder.urls.isEmpty());

        emit tree->activated(childNamed(tree, tree->rootIndex(), QStringLiteral("banana.txt")));
        QCOMPARE(recorder.urls.size(), 1);
        QCOMPARE(QFileInfo(recorder.urls.at(0).toLocalFile()).canonicalFilePath(),
                 QFileInfo(dir.filePath(QStringLiteral("banana.txt"))).canonicalFilePath());
    }

    void menuActsOnTheSelectionOrTheRoot()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        fill(dir);
        FakeContext context;
        context.paths.insert(QStringLiteral("f1"), dir.path());
        std::unique_ptr<QWidget> view(folder::createFileTreeView(context, nullptr));
        view->show();
        QTreeView *tree = treeOf(view.get());
        QTRY_VERIFY(childNamed(tree, tree->rootIndex(), QStringLiteral("src")).isValid());
        const QString root = QDir::cleanPath(dir.path());
        const QString banana = root + QStringLiteral("/banana.txt");

        // Nothing selected: the root.
        trigger(tree, QStringLiteral("showInFiles"));
        QCOMPARE(context.shownFolders, QStringList{root});
        trigger(tree, QStringLiteral("terminalHere"));
        QCOMPARE(context.terminals, QStringList{root});

        // A file: the file itself, or the folder it is in.
        selectRow(tree, childNamed(tree, tree->rootIndex(), QStringLiteral("banana.txt")));
        trigger(tree, QStringLiteral("openInEditor"));
        QCOMPARE(context.programDir, root);
        QCOMPARE(context.program, QStringLiteral("code"));
        QCOMPARE(context.programArguments, (QStringList{QStringLiteral("-n"), banana}));
        trigger(tree, QStringLiteral("showInFiles"));
        QCOMPARE(context.shownFolders.last(), root);
        trigger(tree, QStringLiteral("copyPath"));
        QCOMPARE(QGuiApplication::clipboard()->text(), QDir::toNativeSeparators(banana));
        trigger(tree, QStringLiteral("copyRelativePath"));
        QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("banana.txt"));

        // A folder: the folder.
        selectRow(tree, childNamed(tree, tree->rootIndex(), QStringLiteral("src")));
        trigger(tree, QStringLiteral("terminalHere"));
        QCOMPARE(context.terminals.last(), root + QStringLiteral("/src"));
    }

    void failureShowsItsMessage()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        fill(dir);
        FakeContext context;
        context.paths.insert(QStringLiteral("f1"), dir.path());
        context.error = QStringLiteral("The program could not be started.");
        std::unique_ptr<QWidget> view(folder::createFileTreeView(context, nullptr));
        view->show();
        QTreeView *tree = treeOf(view.get());
        trigger(tree, QStringLiteral("openInEditor"));
        auto *problem = view->findChild<QLabel *>(QStringLiteral("problem"));
        QCOMPARE(problem->text(), context.error);
        QVERIFY(!problem->isHidden());

        context.error.clear();
        trigger(tree, QStringLiteral("openInEditor"));
        QVERIFY(problem->isHidden());
    }
};

QTEST_MAIN(TestFileTree)
#include "tst_file_tree.moc"
