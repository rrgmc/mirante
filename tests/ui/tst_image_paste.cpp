// Tests ImagePaste on a small task: replacing the image of a focused image
// item, on a task or root dashboard; adding a new item to a task without
// changing any dashboard; the refusal on a cell without an image item,
// without an image type, and without a writable store; pasting into an item
// alone; and the PNG file written to files/. Also tests imageFromMime(),
// which reads the image of a drag from another app.
#include "core/Model.h"
#include "core/Store.h"
#include "items/ItemTypeRegistry.h"
#include "ui/ImagePaste.h"

#include <mirante/Module.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QMimeData>
#include <QTemporaryDir>
#include <QUrl>
#include <QTest>

#include <algorithm>
#include <memory>

using namespace mirante;

namespace {

const QString groupId = QStringLiteral("00000000-0000-0000-0000-000000000001");
const QString projectId = QStringLiteral("00000000-0000-0000-0000-000000000002");
const QString taskId = QStringLiteral("00000000-0000-0000-0000-000000000003");
const QString boardId = QStringLiteral("00000000-0000-0000-0000-000000000004");
const QString rootId = QStringLiteral("00000000-0000-0000-0000-000000000005");
const QString pictureId = QStringLiteral("00000000-0000-0000-0000-000000000006");
const QString noteId = QStringLiteral("00000000-0000-0000-0000-000000000007");

const QString pictureType = QStringLiteral("testmod.picture");
const QString noteType = QStringLiteral("testmod.note");
const QString oldFile = QStringLiteral("old.png");

// A type that a pasted image fills, through its "file" field.
class PictureType : public ItemType
{
public:
    QString id() const override { return pictureType; }
    QString name() const override { return QStringLiteral("Picture"); }
    QIcon icon() const override { return {}; }

    std::vector<Field> fields() const override
    {
        Field file;
        file.key = QStringLiteral("file");
        file.label = QStringLiteral("File");
        file.required = true;
        Field memo;
        memo.key = QStringLiteral("memo");
        memo.label = QStringLiteral("Memo");
        memo.defaultValue = QStringLiteral("pasted");
        return {file, memo};
    }

    QString pastedImageField() const override { return QStringLiteral("file"); }
};

// A type without a pastedImageField.
class NoteType : public ItemType
{
public:
    QString id() const override { return noteType; }
    QString name() const override { return QStringLiteral("Note"); }
    QIcon icon() const override { return {}; }
};

void registerWithImages(ModuleRegistrar &registrar)
{
    registrar.addItemType(std::make_unique<PictureType>());
    registrar.addItemType(std::make_unique<NoteType>());
}

void registerWithoutImages(ModuleRegistrar &registrar)
{
    registrar.addItemType(std::make_unique<NoteType>());
}

LayoutNode cell(const std::optional<CellRef> &ref)
{
    LayoutNode node;
    node.kind = LayoutNode::Kind::Cell;
    node.ref = ref;
    return node;
}

CellRef itemRef(const QString &itemId, const QString &task = {})
{
    CellRef ref;
    ref.item = itemId;
    ref.task = task;
    return ref;
}

Item makeItem(const QString &id, const QString &type, const QString &name, const QJsonObject &config)
{
    Item item;
    item.id = id;
    item.type = type;
    item.name = name;
    item.config = config;
    return item;
}

QImage redImage()
{
    QImage image(4, 3, QImage::Format_ARGB32);
    image.fill(Qt::red);
    return image;
}

} // namespace

class TestImagePaste : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir dir;
    Model model;
    std::unique_ptr<Store> store;
    ItemTypeRegistry types;
    ItemTypeRegistry typesWithoutImages;

    const Task &task() const { return *model.task(taskId); }
    const LayoutNode &board() const { return model.taskDashboard(taskId, boardId)->layout; }
    const Dashboard &boardEntity() const { return *model.taskDashboard(taskId, boardId); }
    const RootDashboard &root() const { return *model.rootDashboard(rootId); }
    const Item *itemNamed(const QString &name) const;
    QStringList savedFiles() const;
    QString pictureFile() const;

private slots:
    void initTestCase();
    void init();
    void replaceFocusedImage();
    void addToTask();
    void numberedNames();
    void noImageItem();
    void rootDashboard();
    void noImageType();
    void pasteIntoItem();
    void saveFailed();
    void check();
    void messages();
    void imageFromMime();
};

const Item *TestImagePaste::itemNamed(const QString &name) const
{
    for (const Item &item : task().items) {
        if (item.name == name)
            return &item;
    }
    return nullptr;
}

QStringList TestImagePaste::savedFiles() const
{
    return QDir(dir.filePath(QStringLiteral("files"))).entryList(QDir::Files, QDir::Name);
}

QString TestImagePaste::pictureFile() const
{
    return model.item(taskId, pictureId)->config.value(QStringLiteral("file")).toString();
}

void TestImagePaste::initTestCase()
{
    QVERIFY(dir.isValid());
    types.registerModule(QStringLiteral("testmod"), registerWithImages);
    typesWithoutImages.registerModule(QStringLiteral("testmod"), registerWithoutImages);
    QVERIFY(store::createWorkspace(dir.path(), QStringLiteral("Test")));
    store = std::make_unique<Store>(model);
    QVERIFY(store->open(dir.path()).empty());
    QVERIFY(!store->readOnly());
}

// The task has a picture, a note and an empty cell side by side. A root
// dashboard shows the picture and the task's dashboard.
void TestImagePaste::init()
{
    Group group;
    group.id = groupId;
    group.name = QStringLiteral("Group");
    model.setGroup(group);

    Project project;
    project.id = projectId;
    project.groupId = groupId;
    project.name = QStringLiteral("Project");
    model.setProject(project);

    LayoutNode split;
    split.kind = LayoutNode::Kind::Split;
    split.dir = Qt::Horizontal;
    split.children = {cell(itemRef(pictureId)), cell(itemRef(noteId)), cell(std::nullopt)};
    split.sizes = {1.0 / 3, 1.0 / 3, 1.0 / 3};

    Dashboard dashboard;
    dashboard.id = boardId;
    dashboard.name = QStringLiteral("Main");
    dashboard.layout = split;

    Task t;
    t.id = taskId;
    t.projectId = projectId;
    t.title = QStringLiteral("Task");
    t.items = {makeItem(pictureId, pictureType, QStringLiteral("Image"), {{QStringLiteral("file"), oldFile}}),
               makeItem(noteId, noteType, QStringLiteral("Note"), {})};
    t.dashboards = {dashboard};
    t.defaultDashboardId = boardId;
    model.setTask(t);
    QCOMPARE(layout::cellPaths(board()).size(), size_t(3));

    CellRef taskRef;
    taskRef.task = taskId;
    taskRef.dashboard = boardId;
    LayoutNode rootSplit = split;
    rootSplit.children = {cell(itemRef(pictureId, taskId)), cell(taskRef)};
    rootSplit.sizes = {0.5, 0.5};
    RootDashboard root;
    root.id = rootId;
    root.name = QStringLiteral("Root");
    root.layout = rootSplit;
    model.setRootDashboard(root);
}

// Ctrl+V on a cell with an image item replaces its image. The layout keeps
// its shape, and nothing is added.
void TestImagePaste::replaceFocusedImage()
{
    ImagePaste paste(model, *store, types);
    const LayoutNode before = board();
    const auto result = paste.paste(redImage(), {taskId, boardId}, NodePath{0});
    QCOMPARE(result, ImagePaste::Result::ReplacedImage);

    const QString file = pictureFile();
    QVERIFY(file != oldFile);
    QVERIFY(file.endsWith(QStringLiteral(".png")));
    QVERIFY(QFileInfo::exists(store->filePath(file)));
    QVERIFY(savedFiles().contains(file));
    QCOMPARE(task().items.size(), size_t(2));
    QVERIFY(layout::sameShape(board(), before));

    // The saved file is a PNG of the image.
    QImage saved(store->filePath(file), "PNG");
    QCOMPARE(saved.size(), QSize(4, 3));
    QCOMPARE(saved.pixelColor(0, 0), QColor(Qt::red));
}

// A new item goes to the task, with the type's defaults and the image's file,
// and no dashboard changes.
void TestImagePaste::addToTask()
{
    ImagePaste paste(model, *store, types);
    const Dashboard boardBefore = boardEntity();
    const RootDashboard rootBefore = root();
    QString newId;
    QCOMPARE(paste.addToTask(redImage(), taskId, &newId), ImagePaste::Result::CreatedItem);

    QCOMPARE(task().items.size(), size_t(3));
    const Item *created = itemNamed(QStringLiteral("Image 2"));
    QVERIFY(created);
    QCOMPARE(created->id, newId);
    QCOMPARE(created->type, pictureType);
    QCOMPARE(created->config.value(QStringLiteral("memo")).toString(), QStringLiteral("pasted"));
    const QString file = created->config.value(QStringLiteral("file")).toString();
    QVERIFY(QFileInfo::exists(store->filePath(file)));
    QCOMPARE(QImage(store->filePath(file), "PNG").size(), QSize(4, 3));

    QVERIFY(boardEntity() == boardBefore);
    QVERIFY(root() == rootBefore);
    QCOMPARE(pictureFile(), oldFile);
}

void TestImagePaste::numberedNames()
{
    // "Image 2" is taken, so the next names are "Image 3" and "Image 4".
    Task t = task();
    t.items.push_back(makeItem(QStringLiteral("00000000-0000-0000-0000-000000000008"), noteType,
                               QStringLiteral("Image 2"), {}));
    model.setTask(t);

    ImagePaste paste(model, *store, types);
    QCOMPARE(paste.addToTask(redImage(), taskId), ImagePaste::Result::CreatedItem);
    QCOMPARE(paste.addToTask(redImage(), taskId), ImagePaste::Result::CreatedItem);
    QVERIFY(itemNamed(QStringLiteral("Image 3")));
    QVERIFY(itemNamed(QStringLiteral("Image 4")));
}

// Ctrl+V on a dashboard never adds an item: a cell without an image item, or
// no focused cell, changes nothing.
void TestImagePaste::noImageItem()
{
    ImagePaste paste(model, *store, types);
    const Task before = task();
    const QStringList files = savedFiles();
    QCOMPARE(paste.paste(redImage(), {taskId, boardId}, NodePath{1}), ImagePaste::Result::NoImageItem);
    QCOMPARE(paste.paste(redImage(), {taskId, boardId}, NodePath{2}), ImagePaste::Result::NoImageItem);
    QCOMPARE(paste.paste(redImage(), {taskId, boardId}, std::nullopt), ImagePaste::Result::NoImageItem);
    QCOMPARE(paste.paste(redImage(), {taskId, boardId}, NodePath{7}), ImagePaste::Result::NoImageItem);
    QVERIFY(task() == before);
    QCOMPARE(savedFiles(), files);
}

// On a root dashboard, a cell with an image item has its image replaced. A
// task cell, or no focused cell, adds nothing.
void TestImagePaste::rootDashboard()
{
    ImagePaste paste(model, *store, types);
    const RootDashboard rootBefore = root();
    QCOMPARE(paste.paste(redImage(), {QString(), rootId}, NodePath{1}), ImagePaste::Result::NoImageItem);
    QCOMPARE(paste.paste(redImage(), {QString(), rootId}, std::nullopt), ImagePaste::Result::NoImageItem);
    QCOMPARE(task().items.size(), size_t(2));

    QCOMPARE(paste.paste(redImage(), {QString(), rootId}, NodePath{0}), ImagePaste::Result::ReplacedImage);
    QVERIFY(pictureFile() != oldFile);
    QVERIFY(root() == rootBefore);
}

void TestImagePaste::noImageType()
{
    ImagePaste paste(model, *store, typesWithoutImages);
    const Task before = task();
    const QStringList files = savedFiles();
    QCOMPARE(paste.paste(redImage(), {taskId, boardId}, NodePath{0}), ImagePaste::Result::NoImageType);
    QCOMPARE(paste.paste(redImage(), {taskId, boardId}, NodePath{2}), ImagePaste::Result::NoImageType);
    QCOMPARE(paste.pasteIntoItem(redImage(), taskId, pictureId), ImagePaste::Result::NoImageType);
    QCOMPARE(paste.addToTask(redImage(), taskId), ImagePaste::Result::NoImageType);
    QVERIFY(task() == before);
    QCOMPARE(savedFiles(), files);
}

void TestImagePaste::pasteIntoItem()
{
    ImagePaste paste(model, *store, types);
    QCOMPARE(paste.pasteIntoItem(redImage(), taskId, pictureId), ImagePaste::Result::ReplacedImage);
    const QString file = pictureFile();
    QVERIFY(file != oldFile);
    QVERIFY(savedFiles().contains(file));

    QCOMPARE(paste.pasteIntoItem(redImage(), taskId, noteId), ImagePaste::Result::NoImageType);
    QCOMPARE(paste.pasteIntoItem(redImage(), taskId, QStringLiteral("missing")), ImagePaste::Result::NoImageType);
}

void TestImagePaste::saveFailed()
{
    // A Store with no folder open is read-only, and cannot save a file.
    Model other;
    Store closed(other);
    ImagePaste paste(model, closed, types);
    const Task before = task();
    QCOMPARE(paste.pasteIntoItem(redImage(), taskId, pictureId), ImagePaste::Result::SaveFailed);
    QCOMPARE(paste.paste(redImage(), {taskId, boardId}, NodePath{0}), ImagePaste::Result::SaveFailed);
    QCOMPARE(paste.addToTask(redImage(), taskId), ImagePaste::Result::SaveFailed);
    QVERIFY(task() == before);
    // The checks know it before anything is tried.
    QCOMPARE(paste.checkItem(taskId, pictureId), ImagePaste::Result::SaveFailed);
    QCOMPARE(paste.check({taskId, boardId}, NodePath{0}), ImagePaste::Result::SaveFailed);
    QCOMPARE(paste.checkTask(taskId), ImagePaste::Result::SaveFailed);
}

// The checks say what a paste would do, without saving or changing anything.
void TestImagePaste::check()
{
    ImagePaste paste(model, *store, types);
    const QStringList filesBefore = savedFiles();
    const Task before = task();

    QCOMPARE(paste.check({taskId, boardId}, NodePath{0}), ImagePaste::Result::ReplacedImage);
    QCOMPARE(paste.check({taskId, boardId}, NodePath{1}), ImagePaste::Result::NoImageItem);
    QCOMPARE(paste.check({taskId, boardId}, NodePath{2}), ImagePaste::Result::NoImageItem);
    QCOMPARE(paste.check({taskId, boardId}, std::nullopt), ImagePaste::Result::NoImageItem);
    QCOMPARE(paste.check({QString(), rootId}, NodePath{0}), ImagePaste::Result::ReplacedImage);
    QCOMPARE(paste.check({QString(), rootId}, NodePath{1}), ImagePaste::Result::NoImageItem);
    QCOMPARE(paste.checkItem(taskId, pictureId), ImagePaste::Result::ReplacedImage);
    QCOMPARE(paste.checkItem(taskId, noteId), ImagePaste::Result::NoImageType);
    QCOMPARE(paste.checkTask(taskId), ImagePaste::Result::CreatedItem);
    QCOMPARE(paste.checkTask(QStringLiteral("gone")), ImagePaste::Result::SaveFailed);

    ImagePaste without(model, *store, typesWithoutImages);
    QCOMPARE(without.check({taskId, boardId}, NodePath{2}), ImagePaste::Result::NoImageType);
    QCOMPARE(without.checkTask(taskId), ImagePaste::Result::NoImageType);

    QCOMPARE(savedFiles(), filesBefore);
    QCOMPARE(task().items.size(), before.items.size());
    QCOMPARE(pictureFile(), oldFile);
}

void TestImagePaste::messages()
{
    QVERIFY(ImagePaste::message(ImagePaste::Result::ReplacedImage).isEmpty());
    QVERIFY(ImagePaste::message(ImagePaste::Result::CreatedItem).isEmpty());
    QCOMPARE(ImagePaste::message(ImagePaste::Result::NoImageItem),
             QStringLiteral("To add an image, use Paste Image… in a task's Items menu."));
    QCOMPARE(ImagePaste::message(ImagePaste::Result::NoImageType), QStringLiteral("This build cannot show images."));
    QCOMPARE(ImagePaste::message(ImagePaste::Result::SaveFailed), QStringLiteral("The image could not be saved."));
}

// A drag carries the first image file that can be read, by its suffix, else
// its image data. Other files, files that are not images and links that are
// not local files are skipped.
void TestImagePaste::imageFromMime()
{
    QTemporaryDir files;
    QVERIFY(files.isValid());
    const auto write = [&files](const QString &name, const QImage &image, const char *format) {
        const QString path = files.filePath(name);
        return image.save(path, format) ? QUrl::fromLocalFile(path) : QUrl();
    };
    QImage blue(5, 4, QImage::Format_RGB32);
    blue.fill(Qt::blue);
    QImage green(3, 2, QImage::Format_RGB32);
    green.fill(Qt::green);
    const QUrl png = write(QStringLiteral("blue.png"), blue, "PNG");
    const QUrl jpeg = write(QStringLiteral("green.JPEG"), green, "JPEG");
    const QUrl bmp = write(QStringLiteral("green.bmp"), green, "BMP");
    // An image under a suffix that is not taken.
    const QUrl ico = write(QStringLiteral("blue.ico"), blue, "PNG");
    QVERIFY(!png.isEmpty() && !jpeg.isEmpty() && !bmp.isEmpty() && !ico.isEmpty());
    QFile broken(files.filePath(QStringLiteral("broken.png")));
    QVERIFY(broken.open(QIODevice::WriteOnly));
    broken.write("not a png");
    broken.close();
    const QUrl brokenUrl = QUrl::fromLocalFile(broken.fileName());
    QFile text(files.filePath(QStringLiteral("notes.txt")));
    QVERIFY(text.open(QIODevice::WriteOnly));
    text.write("notes");
    text.close();
    const QUrl textUrl = QUrl::fromLocalFile(text.fileName());

    QVERIFY(ImagePaste::imageFromMime(nullptr).isNull());
    QMimeData empty;
    QVERIFY(ImagePaste::imageFromMime(&empty).isNull());

    // An image file.
    QMimeData file;
    file.setUrls({png});
    QCOMPARE(ImagePaste::imageFromMime(&file).size(), QSize(5, 4));
    QCOMPARE(ImagePaste::imageFromMime(&file).pixelColor(0, 0), QColor(Qt::blue));
    // The suffix is compared without case.
    file.setUrls({jpeg});
    QCOMPARE(ImagePaste::imageFromMime(&file).size(), QSize(3, 2));

    // The first of several that is read: a file that is not an image, one
    // with another suffix, an unreadable one and a link are skipped.
    QMimeData several;
    several.setUrls({textUrl, ico, brokenUrl, QUrl(QStringLiteral("https://example.com/a.png")), bmp, png});
    QCOMPARE(ImagePaste::imageFromMime(&several).size(), QSize(3, 2));

    // Image data, as a browser gives.
    QMimeData data;
    data.setImageData(blue);
    QCOMPARE(ImagePaste::imageFromMime(&data).size(), QSize(5, 4));
    // A file comes before the image data.
    data.setUrls({bmp});
    QCOMPARE(ImagePaste::imageFromMime(&data).size(), QSize(3, 2));
    // Without a readable file, the image data.
    data.setUrls({textUrl, brokenUrl});
    QCOMPARE(ImagePaste::imageFromMime(&data).size(), QSize(5, 4));

    // Only files that are not images, or cannot be read: no image.
    QMimeData others;
    others.setUrls({textUrl, brokenUrl, ico});
    QVERIFY(ImagePaste::imageFromMime(&others).isNull());
    QMimeData words;
    words.setText(QStringLiteral("blue.png"));
    QVERIFY(ImagePaste::imageFromMime(&words).isNull());
}

QTEST_MAIN(TestImagePaste)
#include "tst_image_paste.moc"
