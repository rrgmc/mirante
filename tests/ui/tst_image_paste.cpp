// Tests ImagePaste on a small task: replacing the image of a focused image
// item, even on a locked dashboard; creating a new item in an empty focused
// cell, beside a filled one, or beside the whole layout; the refusals on a
// locked or root dashboard, without an image type, and without a writable
// store; pasting into an item alone; and the PNG file written to files/.
#include "core/Model.h"
#include "core/Store.h"
#include "items/ItemTypeRegistry.h"
#include "layout/LockState.h"
#include "ui/ImagePaste.h"

#include <mirante/Module.h>

#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QTemporaryDir>
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
    LockState locks;

    const Task &task() const { return *model.task(taskId); }
    const LayoutNode &board() const { return model.taskDashboard(taskId, boardId)->layout; }
    const Item *itemNamed(const QString &name) const;
    QStringList savedFiles() const;
    QString pictureFile() const;

private slots:
    void initTestCase();
    void init();
    void replaceWhileLocked();
    void createInEmptyCell();
    void splitFilledCell();
    void splitRootWithoutFocus();
    void fillOnlyEmptyCellWithoutFocus();
    void numberedNames();
    void lockedRefuses();
    void rootDashboard();
    void noImageType();
    void pasteIntoItem();
    void saveFailed();
    void messages();
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

    locks.setUnlocked(boardId, false);
    locks.setUnlocked(rootId, false);
}

void TestImagePaste::replaceWhileLocked()
{
    ImagePaste paste(model, *store, types, locks);
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

void TestImagePaste::createInEmptyCell()
{
    locks.setUnlocked(boardId, true);
    ImagePaste paste(model, *store, types, locks);
    const auto result = paste.paste(redImage(), {taskId, boardId}, NodePath{2});
    QCOMPARE(result, ImagePaste::Result::CreatedItem);

    QCOMPARE(task().items.size(), size_t(3));
    const Item *created = itemNamed(QStringLiteral("Image 2"));
    QVERIFY(created);
    QCOMPARE(created->type, pictureType);
    QCOMPARE(created->config.value(QStringLiteral("memo")).toString(), QStringLiteral("pasted"));
    const QString file = created->config.value(QStringLiteral("file")).toString();
    QVERIFY(QFileInfo::exists(store->filePath(file)));

    QCOMPARE(layout::cellPaths(board()).size(), size_t(3));
    const LayoutNode *node = layout::nodeAt(board(), NodePath{2});
    QVERIFY(node && node->ref);
    QCOMPARE(node->ref->item, created->id);
}

void TestImagePaste::splitFilledCell()
{
    locks.setUnlocked(boardId, true);
    ImagePaste paste(model, *store, types, locks);
    const auto result = paste.paste(redImage(), {taskId, boardId}, NodePath{1});
    QCOMPARE(result, ImagePaste::Result::CreatedItem);

    const Item *created = itemNamed(QStringLiteral("Image 2"));
    QVERIFY(created);
    // The new cell is to the right of the note, before the empty cell.
    const std::vector<NodePath> cells = layout::cellPaths(board());
    QCOMPARE(cells.size(), size_t(4));
    std::vector<std::optional<CellRef>> refs;
    for (const NodePath &path : cells)
        refs.push_back(layout::nodeAt(board(), path)->ref);
    QCOMPARE(refs[0]->item, pictureId);
    QCOMPARE(refs[1]->item, noteId);
    QVERIFY(refs[2].has_value());
    QCOMPARE(refs[2]->item, created->id);
    QVERIFY(!refs[3].has_value());
}

void TestImagePaste::splitRootWithoutFocus()
{
    locks.setUnlocked(boardId, true);
    ImagePaste paste(model, *store, types, locks);
    const auto result = paste.paste(redImage(), {taskId, boardId}, std::nullopt);
    QCOMPARE(result, ImagePaste::Result::CreatedItem);

    const Item *created = itemNamed(QStringLiteral("Image 2"));
    QVERIFY(created);
    const std::vector<NodePath> cells = layout::cellPaths(board());
    QCOMPARE(cells.size(), size_t(4));
    const LayoutNode *last = layout::nodeAt(board(), cells.back());
    QVERIFY(last->ref);
    QCOMPARE(last->ref->item, created->id);
}

void TestImagePaste::fillOnlyEmptyCellWithoutFocus()
{
    locks.setUnlocked(boardId, true);
    model.setTaskDashboardLayout(taskId, boardId, LayoutNode{});
    ImagePaste paste(model, *store, types, locks);
    QCOMPARE(paste.paste(redImage(), {taskId, boardId}, std::nullopt), ImagePaste::Result::CreatedItem);

    const Item *created = itemNamed(QStringLiteral("Image 2"));
    QVERIFY(created);
    QCOMPARE(board().kind, LayoutNode::Kind::Cell);
    QVERIFY(board().ref);
    QCOMPARE(board().ref->item, created->id);
}

void TestImagePaste::numberedNames()
{
    locks.setUnlocked(boardId, true);
    // "Image 2" is taken, so the next names are "Image 3" and "Image 4".
    Task t = task();
    t.items.push_back(makeItem(QStringLiteral("00000000-0000-0000-0000-000000000008"), noteType,
                               QStringLiteral("Image 2"), {}));
    model.setTask(t);

    ImagePaste paste(model, *store, types, locks);
    QCOMPARE(paste.paste(redImage(), {taskId, boardId}, NodePath{2}), ImagePaste::Result::CreatedItem);
    QCOMPARE(paste.paste(redImage(), {taskId, boardId}, NodePath{1}), ImagePaste::Result::CreatedItem);
    QVERIFY(itemNamed(QStringLiteral("Image 3")));
    QVERIFY(itemNamed(QStringLiteral("Image 4")));
}

void TestImagePaste::lockedRefuses()
{
    ImagePaste paste(model, *store, types, locks);
    const Task before = task();
    const QStringList files = savedFiles();
    QCOMPARE(paste.paste(redImage(), {taskId, boardId}, NodePath{1}), ImagePaste::Result::Locked);
    QCOMPARE(paste.paste(redImage(), {taskId, boardId}, NodePath{2}), ImagePaste::Result::Locked);
    QCOMPARE(paste.paste(redImage(), {taskId, boardId}, std::nullopt), ImagePaste::Result::Locked);
    QVERIFY(task() == before);
    QCOMPARE(savedFiles(), files);
}

void TestImagePaste::rootDashboard()
{
    locks.setUnlocked(rootId, true);
    ImagePaste paste(model, *store, types, locks);
    const QStringList files = savedFiles();
    QCOMPARE(paste.paste(redImage(), {QString(), rootId}, NodePath{1}), ImagePaste::Result::RootDashboard);
    QCOMPARE(paste.paste(redImage(), {QString(), rootId}, std::nullopt), ImagePaste::Result::RootDashboard);
    QCOMPARE(savedFiles(), files);

    // A root dashboard cell showing an image item still has its image replaced.
    locks.setUnlocked(rootId, false);
    QCOMPARE(paste.paste(redImage(), {QString(), rootId}, NodePath{0}), ImagePaste::Result::ReplacedImage);
    QVERIFY(pictureFile() != oldFile);
}

void TestImagePaste::noImageType()
{
    locks.setUnlocked(boardId, true);
    ImagePaste paste(model, *store, typesWithoutImages, locks);
    const Task before = task();
    const QStringList files = savedFiles();
    QCOMPARE(paste.paste(redImage(), {taskId, boardId}, NodePath{0}), ImagePaste::Result::NoImageType);
    QCOMPARE(paste.paste(redImage(), {taskId, boardId}, NodePath{2}), ImagePaste::Result::NoImageType);
    QCOMPARE(paste.pasteIntoItem(redImage(), taskId, pictureId), ImagePaste::Result::NoImageType);
    QVERIFY(task() == before);
    QCOMPARE(savedFiles(), files);
}

void TestImagePaste::pasteIntoItem()
{
    ImagePaste paste(model, *store, types, locks);
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
    ImagePaste paste(model, closed, types, locks);
    const Task before = task();
    QCOMPARE(paste.pasteIntoItem(redImage(), taskId, pictureId), ImagePaste::Result::SaveFailed);
    locks.setUnlocked(boardId, true);
    QCOMPARE(paste.paste(redImage(), {taskId, boardId}, NodePath{2}), ImagePaste::Result::SaveFailed);
    QVERIFY(task() == before);
}

void TestImagePaste::messages()
{
    QVERIFY(ImagePaste::message(ImagePaste::Result::ReplacedImage).isEmpty());
    QVERIFY(ImagePaste::message(ImagePaste::Result::CreatedItem).isEmpty());
    QCOMPARE(ImagePaste::message(ImagePaste::Result::Locked),
             QStringLiteral("Unlock the dashboard to paste an image into it."));
    QCOMPARE(ImagePaste::message(ImagePaste::Result::RootDashboard),
             QStringLiteral("Paste images into a task dashboard."));
    QCOMPARE(ImagePaste::message(ImagePaste::Result::NoImageType), QStringLiteral("This build cannot show images."));
    QCOMPARE(ImagePaste::message(ImagePaste::Result::SaveFailed), QStringLiteral("The image could not be saved."));
}

QTEST_MAIN(TestImagePaste)
#include "tst_image_paste.moc"
