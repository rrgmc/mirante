// Tests local files dropped on a task in the sidebar, other than images: the
// first type whose fromFile() takes the file adds an item, a match with a
// copyKey copies the file into files/ with the workspace's size limits, and
// a file no type takes, or read-only data, says why in the status bar. See
// docs/decisions/0104-document-items.md.
#include "core/DeviceSettings.h"
#include "core/Model.h"
#include "core/Store.h"
#include "items/ItemTypeRegistry.h"
#include "ui/EntityActions.h"
#include "ui/MainWindow.h"
#include "ui/SidebarModel.h"
#include "ui/SidebarTree.h"
#include "ui/WorkspaceFiles.h"

#include <mirante/FileMatch.h>
#include <mirante/ItemType.h>
#include <mirante/Module.h>

#include <QApplication>
#include <QDir>
#include <QDirIterator>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileInfo>
#include <QMimeData>
#include <QSettings>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include <memory>

using namespace mirante;

namespace {

// The Overview task, of the home project, which has no folders.
const QString kHomeTask = QStringLiteral("d71f3227-79a2-55e9-a0d8-429b99432282");
// A task of the backend project, whose main folder is kBackendFolder.
const QString kBackendTask = QStringLiteral("02fc9a52-653c-562f-a3a2-6d5a4ca98623");
const QString kBackendFolder = QStringLiteral("622f4d51-b29b-5213-82cf-6a85939c8b06");

bool isText(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix();
    return suffix == QLatin1String("md") || suffix == QLatin1String("pdf");
}

// Takes a known file inside one of the project's folders.
class ProjectFileType : public ItemType
{
public:
    QString id() const override { return QStringLiteral("docs.project"); }
    QString name() const override { return QStringLiteral("Project file"); }
    QIcon icon() const override { return {}; }
    std::vector<Field> fields() const override
    {
        Field format;
        format.key = QStringLiteral("format");
        format.defaultValue = QStringLiteral("auto");
        return {format};
    }
    std::optional<FileMatch> fromFile(const FileDrop &drop) const override
    {
        if (!isText(drop.path))
            return std::nullopt;
        for (const auto &[id, path] : drop.folders) {
            const QString relative = QDir(path).relativeFilePath(drop.path);
            if (!relative.startsWith(QLatin1String("..")))
                return FileMatch{{{QStringLiteral("folder"), id}, {QStringLiteral("path"), relative}},
                                 QFileInfo(drop.path).fileName(), {}};
        }
        return std::nullopt;
    }
};

// Takes a known file anywhere else, as a copy in files/.
class WorkspaceFileType : public ItemType
{
public:
    QString id() const override { return QStringLiteral("docs.workspace"); }
    QString name() const override { return QStringLiteral("Workspace file"); }
    QIcon icon() const override { return {}; }
    std::optional<FileMatch> fromFile(const FileDrop &drop) const override
    {
        if (!isText(drop.path))
            return std::nullopt;
        for (const auto &folder : drop.folders) {
            if (!QDir(folder.second).relativeFilePath(drop.path).startsWith(QLatin1String("..")))
                return std::nullopt;
        }
        return FileMatch{{{QStringLiteral("originalName"), QFileInfo(drop.path).fileName()}},
                         QFileInfo(drop.path).fileName(), QStringLiteral("file")};
    }
};

void registerDocs(ModuleRegistrar &registrar)
{
    registrar.addItemType(std::make_unique<ProjectFileType>());
    registrar.addItemType(std::make_unique<WorkspaceFileType>());
}

bool copyFixtures(const QString &target)
{
    QDirIterator files(QStringLiteral(MIRANTE_FIXTURES_DIR), QDir::Files, QDirIterator::Subdirectories);
    while (files.hasNext()) {
        const QString source = files.next();
        const QString relative = QDir(QStringLiteral(MIRANTE_FIXTURES_DIR)).relativeFilePath(source);
        if (!QDir(target).mkpath(QFileInfo(relative).path()) || !QFile::copy(source, QDir(target).filePath(relative)))
            return false;
    }
    return true;
}

bool writeFile(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

// A file of the given size, written as zeros without holding them in memory.
bool sizedFile(const QString &path, qint64 size)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.resize(size);
}

struct Fixture
{
    QTemporaryDir settingsDir;
    QSettings iniSettings{settingsDir.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat};
    DeviceSettings settings{&iniSettings};
    ItemTypeRegistry types;
    Model model;
    Store dataStore{model};
    QTemporaryDir workspace;
    std::unique_ptr<MainWindow> window;
    QStringList asked;
    QStringList told;
    bool answer = true;

    Fixture()
    {
        store::load(QStringLiteral(MIRANTE_FIXTURES_DIR), model);
        types.registerModule(QStringLiteral("docs"), &registerDocs);
        window = std::make_unique<MainWindow>(model, types, dataStore, settings, nullptr, nullptr);
        window->entityActions()->setFileMessages(
            [this](const QString &question) {
                asked << question;
                return answer;
            },
            [this](const QString &text) { told << text; });
        window->resize(1200, 800);
        window->show();
    }

    bool writable()
    {
        if (!workspace.isValid() || !copyFixtures(workspace.path()))
            return false;
        window->openWorkspace(workspace.path());
        return !dataStore.readOnly();
    }

    SidebarTree *tree() const { return window->findChild<SidebarTree *>(); }

    QPoint rowCentre(const QString &id) const
    {
        auto *sidebar = qobject_cast<SidebarModel *>(tree()->model());
        const QModelIndex index = sidebar->indexOf(id);
        tree()->scrollTo(index);
        return tree()->visualRect(index).center();
    }

    EntityActions *actions() const { return window->entityActions(); }
};

Qt::DropAction dropFile(QWidget *target, const QPoint &pos, const QString &path)
{
    QMimeData mime;
    mime.setUrls({QUrl::fromLocalFile(path)});
    const Qt::DropActions actions = Qt::CopyAction | Qt::MoveAction;
    QDragEnterEvent enter(pos, actions, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(target, &enter);
    if (!enter.isAccepted())
        return Qt::IgnoreAction;
    QDragMoveEvent move(pos, actions, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(target, &move);
    if (!move.isAccepted())
        return Qt::IgnoreAction;
    QDropEvent drop(QPointF(pos), actions, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(target, &drop);
    return drop.isAccepted() ? drop.dropAction() : Qt::IgnoreAction;
}

} // namespace

class TestFileDrops : public QObject
{
    Q_OBJECT

private slots:
    // A PDF dropped on a task of a project without folders is copied into
    // files/, with its suffix, and the item names it.
    void sidebarDropCopiesIntoTheWorkspace()
    {
        Fixture f;
        QVERIFY(f.writable());
        QTemporaryDir outside;
        const QString path = outside.filePath(QStringLiteral("datasheet.pdf"));
        QVERIFY(writeFile(path, "%PDF-1.4 test"));
        const size_t before = f.model.task(kHomeTask)->items.size();

        QCOMPARE(dropFile(f.tree()->viewport(), f.rowCentre(kHomeTask), path), Qt::CopyAction);
        QTRY_COMPARE(f.model.task(kHomeTask)->items.size(), before + 1);
        const Item &item = f.model.task(kHomeTask)->items.back();
        QCOMPARE(item.type, QStringLiteral("docs.workspace"));
        QCOMPARE(item.name, QStringLiteral("datasheet.pdf"));
        QCOMPARE(item.config.value(QStringLiteral("originalName")).toString(), QStringLiteral("datasheet.pdf"));
        const QString copy = item.config.value(QStringLiteral("file")).toString();
        QVERIFY(copy.endsWith(QLatin1String(".pdf")));
        QFile copied(f.dataStore.filePath(copy));
        QVERIFY(copied.open(QIODevice::ReadOnly));
        QCOMPARE(copied.readAll(), QByteArray("%PDF-1.4 test"));
        QVERIFY(QFileInfo::exists(path));
        QVERIFY(f.told.isEmpty());
    }

    // A file inside one of the project's folders is referred to, not copied,
    // and keeps the fields' defaults for what the match leaves out.
    void fileInAProjectFolderIsReferredTo()
    {
        Fixture f;
        QVERIFY(f.writable());
        QTemporaryDir folder;
        f.settings.setFolderPath(kBackendFolder, folder.path());
        QVERIFY(QDir(folder.path()).mkpath(QStringLiteral("docs")));
        const QString path = folder.filePath(QStringLiteral("docs/design.md"));
        QVERIFY(writeFile(path, "# Design"));

        const QString id = f.actions()->addFile(kBackendTask, path);
        QVERIFY(!id.isEmpty());
        const Item *item = f.model.item(kBackendTask, id);
        QVERIFY(item);
        QCOMPARE(item->type, QStringLiteral("docs.project"));
        QCOMPARE(item->config.value(QStringLiteral("folder")).toString(), kBackendFolder);
        QCOMPARE(item->config.value(QStringLiteral("path")).toString(), QStringLiteral("docs/design.md"));
        QCOMPARE(item->config.value(QStringLiteral("format")).toString(), QStringLiteral("auto"));
        QVERIFY(!item->config.contains(QStringLiteral("file")));
    }

    // A file no type takes changes nothing and says so.
    void unknownFileSaysSo()
    {
        Fixture f;
        QVERIFY(f.writable());
        QTemporaryDir outside;
        const QString path = outside.filePath(QStringLiteral("archive.xyz"));
        QVERIFY(writeFile(path, "x"));
        const Task before = *f.model.task(kHomeTask);
        QCOMPARE(dropFile(f.tree()->viewport(), f.rowCentre(kHomeTask), path), Qt::CopyAction);
        QTRY_COMPARE(f.window->statusBar()->currentMessage(), QStringLiteral("No item type can show this file."));
        QVERIFY(*f.model.task(kHomeTask) == before);
    }

    // The sample data, read-only, adds nothing and says why.
    void readOnlySaysSo()
    {
        Fixture f;
        QVERIFY(f.dataStore.readOnly());
        QTemporaryDir outside;
        const QString path = outside.filePath(QStringLiteral("notes.md"));
        QVERIFY(writeFile(path, "x"));
        QVERIFY(f.actions()->addFile(kHomeTask, path).isEmpty());
        QTRY_COMPARE(f.window->statusBar()->currentMessage(),
                     QStringLiteral("This workspace is read-only, so no item can be added."));
    }

    // Above 20 MB the copy asks first, and a "no" adds nothing; above 50 MB
    // it is refused with a message.
    void largeFilesAskOrAreRefused()
    {
        Fixture f;
        QVERIFY(f.writable());
        QTemporaryDir outside;
        const size_t before = f.model.task(kHomeTask)->items.size();

        const QString large = outside.filePath(QStringLiteral("large.pdf"));
        QVERIFY(sizedFile(large, workspacefiles::kAskAbove + 1));
        f.answer = false;
        QVERIFY(f.actions()->addFile(kHomeTask, large).isEmpty());
        QCOMPARE(f.asked.size(), 1);
        QVERIFY(f.told.isEmpty());
        QCOMPARE(f.model.task(kHomeTask)->items.size(), before);

        f.answer = true;
        QVERIFY(!f.actions()->addFile(kHomeTask, large).isEmpty());
        QCOMPARE(f.asked.size(), 2);
        QCOMPARE(f.model.task(kHomeTask)->items.size(), before + 1);

        const QString huge = outside.filePath(QStringLiteral("huge.pdf"));
        QVERIFY(sizedFile(huge, workspacefiles::kRefuseAbove + 1));
        QVERIFY(f.actions()->addFile(kHomeTask, huge).isEmpty());
        QCOMPARE(f.asked.size(), 2);
        QCOMPARE(f.told.size(), 1);
        QVERIFY(f.told.front().contains(QLatin1String("too large")));
        QCOMPARE(f.model.task(kHomeTask)->items.size(), before + 1);
    }
};

QTEST_MAIN(TestFileDrops)
#include "tst_file_drops.moc"
