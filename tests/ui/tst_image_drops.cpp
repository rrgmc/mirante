// Tests images dropped from other apps on MainWindow, on the fixtures: on a
// task or its Items row in the sidebar, a new Image item after the paste
// dialog; on an Image item's cell, its image replaced after the dialog, on
// task and root dashboards, locked or not, and alone in a tab. Drops
// anywhere else are refused, the read-only sample data says why as a paste
// does, and the sidebar's and the layout editor's own drags still work. See
// docs/decisions/0085-drop-images-from-other-apps.md.
#include "core/DeviceSettings.h"
#include "core/Model.h"
#include "core/Store.h"
#include "items/ItemTypeRegistry.h"
#include "layout/CellFrame.h"
#include "layout/DragMime.h"
#include "layout/LayoutView.h"
#include "layout/TaskCell.h"
#include "ui/EntityActions.h"
#include "ui/ImagePaste.h"
#include "ui/ImagePasteDialog.h"
#include "ui/MainWindow.h"
#include "ui/SidebarModel.h"
#include "ui/SidebarTree.h"

#include <mirante/ItemType.h>
#include <mirante/Module.h>

#include <QApplication>
#include <QDir>
#include <QDirIterator>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QMimeData>
#include <QPainter>
#include <QSettings>
#include <QSignalSpy>
#include <QStatusBar>
#include <QTabBar>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include <memory>

using namespace mirante;

namespace {

// The Overview task has an image item, Garden sketch, and a Notes item.
const QString kImageTask = QStringLiteral("d71f3227-79a2-55e9-a0d8-429b99432282");
const QString kOverviewDashboard = QStringLiteral("b5a2ba74-05a8-5dea-9356-f0e70841de10");
const QString kGardenSketch = QStringLiteral("734a1531-d2f6-5ef8-9585-4233708ba49f");
const QString kNotes = QStringLiteral("422f2d4c-e27c-594c-bf2e-f1a5bfb7cf50");

// A stand-in for the image module's image type, which the ui tests do not link.
class TestImage : public ItemType
{
public:
    QString id() const override { return QStringLiteral("image"); }
    QString name() const override { return QStringLiteral("Image"); }
    QIcon icon() const override { return {}; }
    QString pastedImageField() const override { return QStringLiteral("file"); }
};

void registerTestImage(ModuleRegistrar &registrar) { registrar.addItemType(std::make_unique<TestImage>()); }

// Copies the fixtures into a folder, so a test can open them as a workspace
// that is written to.
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

// The window on the fixtures, loaded without the Store, so the data is
// read-only, as the sample data is, until writable() opens a copy. The image
// type is registered, and the settings live in a temporary INI file.
struct Fixture
{
    QTemporaryDir settingsDir;
    QSettings iniSettings{settingsDir.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat};
    DeviceSettings settings{&iniSettings};
    ItemTypeRegistry types;
    Model model;
    Store dataStore{model};
    QTemporaryDir workspace;    // goes after the window, which watches it
    std::unique_ptr<MainWindow> window;
    // The class names of the dialogs opened; each is accepted if it is the
    // paste dialog, cropped to crop if that is set.
    QStringList dialogs;
    QRect crop;

    Fixture()
    {
        const auto problems = store::load(QStringLiteral(MIRANTE_FIXTURES_DIR), model);
        if (!problems.empty())
            qWarning() << "fixture problems:" << problems.size();
        types.registerModule(QStringLiteral("image"), &registerTestImage);
        window = std::make_unique<MainWindow>(model, types, dataStore, settings, nullptr, nullptr);
        const auto exec = [this](QDialog &d) {
            dialogs << QString::fromLatin1(d.metaObject()->className());
            auto *dialog = qobject_cast<ImagePasteDialog *>(&d);
            if (dialog && crop.isValid())
                dialog->setCrop(crop);
            return dialog != nullptr;
        };
        window->setExec(exec);
        window->entityActions()->setExec(exec);
        window->resize(1200, 800);
        window->show();
    }

    // Opens a writable copy of the fixtures.
    bool writable()
    {
        if (!workspace.isValid() || !copyFixtures(workspace.path()))
            return false;
        window->openWorkspace(workspace.path());
        return !dataStore.readOnly();
    }

    SidebarTree *tree() const { return window->findChild<SidebarTree *>(); }

    // The sidebar row of an id, scrolled into view.
    QModelIndex row(const QString &id) const
    {
        auto *sidebar = qobject_cast<SidebarModel *>(tree()->model());
        const QModelIndex index = sidebar->indexOf(id);
        tree()->scrollTo(index);
        return index;
    }

    // The Items row of a task, expanded and scrolled into view.
    QModelIndex itemsRow(const QString &taskId) const
    {
        const QModelIndex task = row(taskId);
        tree()->expand(task);
        for (int i = 0; i < tree()->model()->rowCount(task); ++i) {
            const QModelIndex child = tree()->model()->index(i, 0, task);
            if (SidebarModel::Kind(child.data(SidebarModel::KindRole).toInt()) == SidebarModel::Kind::ItemList) {
                tree()->scrollTo(child);
                return child;
            }
        }
        return {};
    }

    QPoint centre(const QModelIndex &index) const { return tree()->visualRect(index).center(); }

    QString file() const
    {
        return model.item(kImageTask, kGardenSketch)->config.value(QStringLiteral("file")).toString();
    }
};

// Sends a drag over target at pos and drops it there, as a drag from another
// app does. Returns the action of an accepted drop, or IgnoreAction for a
// drag refused at any step: a refused move shows the forbidden pointer.
Qt::DropAction dragAndDrop(QWidget *target, const QPoint &pos, const QMimeData *mime)
{
    const Qt::DropActions actions = Qt::CopyAction | Qt::MoveAction;
    QDragEnterEvent enter(pos, actions, mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(target, &enter);
    if (!enter.isAccepted())
        return Qt::IgnoreAction;
    QDragMoveEvent move(pos, actions, mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(target, &move);
    if (!move.isAccepted()) {
        QDragLeaveEvent leave;
        QApplication::sendEvent(target, &leave);
        return Qt::IgnoreAction;
    }
    QDropEvent drop(QPointF(pos), actions, mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(target, &drop);
    return drop.isAccepted() ? drop.dropAction() : Qt::IgnoreAction;
}

Qt::DropAction dragAndDrop(QWidget *target, const QMimeData *mime)
{
    return dragAndDrop(target, target->rect().center(), mime);
}

// The cell of an item in a view, or in the task cells of a root dashboard.
CellFrame *itemCell(const LayoutView *view, const QString &itemId)
{
    for (QWidget *cell : view->visibleCells()) {
        if (auto *frame = qobject_cast<CellFrame *>(cell)) {
            if (frame->ref() && frame->ref()->item == itemId)
                return frame;
        } else if (auto *taskCell = qobject_cast<TaskCell *>(cell)) {
            if (const LayoutView *inner = taskCell->view()) {
                if (CellFrame *found = itemCell(inner, itemId))
                    return found;
            }
        }
    }
    return nullptr;
}

// A 300 × 200 blue image with a green rectangle at (100, 50, 40, 30).
QImage picture()
{
    QImage image(300, 200, QImage::Format_RGB32);
    image.fill(Qt::blue);
    QPainter(&image).fillRect(QRect(100, 50, 40, 30), Qt::green);
    return image;
}

// A drag of a PNG file, as from a file manager.
std::unique_ptr<QMimeData> fileDrag(const QTemporaryDir &dir)
{
    const QString path = dir.filePath(QStringLiteral("dropped.png"));
    if (!QFileInfo::exists(path))
        picture().save(path, "PNG");
    auto mime = std::make_unique<QMimeData>();
    mime->setUrls({QUrl::fromLocalFile(path)});
    return mime;
}

// A drag of image data, as from a browser.
std::unique_ptr<QMimeData> dataDrag()
{
    auto mime = std::make_unique<QMimeData>();
    mime->setImageData(picture());
    return mime;
}

} // namespace

class TestImageDrops : public QObject
{
    Q_OBJECT

private slots:
    // A file dropped on a task row, and image data dropped on its Items row,
    // each become a new Image item of the task after the paste dialog, as
    // Paste Image… does. The drop is a copy, so a file manager keeps the file.
    void sidebarDropAddsItems()
    {
        Fixture f;
        QVERIFY(f.writable());
        QTemporaryDir files;
        const size_t before = f.model.task(kImageTask)->items.size();
        f.crop = QRect(100, 50, 40, 30);

        const auto file = fileDrag(files);
        QCOMPARE(dragAndDrop(f.tree()->viewport(), f.centre(f.row(kImageTask)), file.get()), Qt::CopyAction);
        // The dialog opens once the drop is over.
        QVERIFY(f.dialogs.isEmpty());
        QTRY_COMPARE(f.dialogs, QStringList{QStringLiteral("mirante::ImagePasteDialog")});
        const Task *task = f.model.task(kImageTask);
        QCOMPARE(task->items.size(), before + 1);
        QCOMPARE(task->items.back().type, QStringLiteral("image"));
        const QImage saved(f.dataStore.filePath(task->items.back().config.value(QStringLiteral("file")).toString()));
        QCOMPARE(saved.size(), QSize(40, 30));
        QCOMPARE(saved.pixelColor(0, 0), QColor(Qt::green));
        QVERIFY(QFileInfo::exists(files.filePath(QStringLiteral("dropped.png"))));

        f.crop = QRect();
        const auto data = dataDrag();
        QCOMPARE(dragAndDrop(f.tree()->viewport(), f.centre(f.itemsRow(kImageTask)), data.get()), Qt::CopyAction);
        QTRY_COMPARE(f.dialogs.size(), 2);
        task = f.model.task(kImageTask);
        QCOMPARE(task->items.size(), before + 2);
        QCOMPARE(QImage(f.dataStore.filePath(task->items.back().config.value(QStringLiteral("file")).toString())).size(),
                 QSize(300, 200));
    }

    // Other rows refuse an image while it is over them, as do drags without
    // an image and the Trash list. Nothing opens and nothing changes.
    void sidebarRefusesElsewhere()
    {
        Fixture f;
        QVERIFY(f.writable());
        const Task before = *f.model.task(kImageTask);
        const auto data = dataDrag();
        const Project *project = f.model.project(before.projectId);
        QVERIFY(project);
        QWidget *viewport = f.tree()->viewport();

        QCOMPARE(dragAndDrop(viewport, f.centre(f.row(project->id)), data.get()), Qt::IgnoreAction);
        QCOMPARE(dragAndDrop(viewport, f.centre(f.row(project->groupId)), data.get()), Qt::IgnoreAction);
        QCOMPARE(dragAndDrop(viewport, f.centre(f.row(kGardenSketch)), data.get()), Qt::IgnoreAction);

        QMimeData text;
        text.setText(QStringLiteral("not an image"));
        QCOMPARE(dragAndDrop(viewport, f.centre(f.row(kImageTask)), &text), Qt::IgnoreAction);

        auto *tabs = f.window->findChild<QTabBar *>(QStringLiteral("taskStateTabs"));
        QVERIFY(tabs);
        tabs->setCurrentIndex(tabs->count() - 1);
        QCOMPARE(dragAndDrop(viewport, viewport->rect().topLeft() + QPoint(40, 10), data.get()), Qt::IgnoreAction);

        QCoreApplication::processEvents();
        QVERIFY(f.dialogs.isEmpty());
        QVERIFY(*f.model.task(kImageTask) == before);
    }

    // The sidebar's own drags still reorder it.
    void sidebarOwnDrags()
    {
        Fixture f;
        auto *sidebar = qobject_cast<SidebarModel *>(f.tree()->model());
        QSignalSpy moves(sidebar, &SidebarModel::moveRequested);
        const QModelIndex task = f.row(kImageTask);
        std::unique_ptr<QMimeData> mime(sidebar->mimeData({task}));
        QVERIFY(mime);
        QVERIFY(dragAndDrop(f.tree()->viewport(), f.centre(task), mime.get()) != Qt::IgnoreAction);
        QCOMPARE(moves.count(), 1);
        QCOMPARE(moves.front().at(1).toString(), kImageTask);
        QCoreApplication::processEvents();
        QVERIFY(f.dialogs.isEmpty());
    }

    // In read-only data, such as the sample data, a drop on a task says why
    // in the status bar and opens no dialog, as Paste Image… does.
    void sidebarReadOnly()
    {
        Fixture f;
        QVERIFY(f.dataStore.readOnly());
        const size_t before = f.model.task(kImageTask)->items.size();
        const auto data = dataDrag();
        QCOMPARE(dragAndDrop(f.tree()->viewport(), f.centre(f.row(kImageTask)), data.get()), Qt::CopyAction);
        QTRY_COMPARE(f.window->statusBar()->currentMessage(), ImagePaste::message(ImagePaste::Result::SaveFailed));
        QVERIFY(f.dialogs.isEmpty());
        QCOMPARE(f.model.task(kImageTask)->items.size(), before);
    }

    // A drop on an Image item's cell replaces its image after the paste
    // dialog, as Ctrl+V does: on a task dashboard, locked or not, in a root
    // dashboard, and with the item alone in a tab. Cancel changes nothing.
    void cellDropReplacesImage()
    {
        Fixture f;
        QVERIFY(f.writable());
        QTemporaryDir files;
        const auto file = fileDrag(files);
        const auto drop = [&f, &file](const QString &where) {
            const QString before = f.file();
            const qsizetype dialogs = f.dialogs.size();
            CellFrame *cell = itemCell(f.window->currentLayoutView(), kGardenSketch);
            if (!cell)
                return where + QStringLiteral(": no cell");
            if (dragAndDrop(cell, file.get()) != Qt::CopyAction)
                return where + QStringLiteral(": refused");
            if (!QTest::qWaitFor([&f, dialogs] { return f.dialogs.size() == dialogs + 1; }))
                return where + QStringLiteral(": no dialog");
            if (f.file() == before)
                return where + QStringLiteral(": unchanged");
            return QString();
        };

        // The Overview task dashboard shows the image item.
        LayoutNode cell;
        cell.kind = LayoutNode::Kind::Cell;
        cell.ref = CellRef{kGardenSketch, QString(), QString()};
        f.model.setTaskDashboardLayout(kImageTask, kOverviewDashboard, cell);
        f.window->openTaskPreview(kImageTask, kOverviewDashboard);
        const bool editing = f.window->currentLayoutView()->editMode();
        QCOMPARE(drop(QStringLiteral("task dashboard")), QString());
        f.window->toggleCurrentLock();
        QVERIFY(f.window->currentLayoutView()->editMode() != editing);
        QCOMPARE(drop(QStringLiteral("task dashboard, lock toggled")), QString());

        // A root dashboard with a cell of the item, locked or not.
        RootDashboard root;
        root.id = QStringLiteral("00000000-0000-0000-0000-0000000000d1");
        root.name = QStringLiteral("Drops");
        root.layout.ref = CellRef{kGardenSketch, kImageTask, QString()};
        f.model.addRootDashboard(root);
        f.window->openRootDashboard(root.id);
        QCOMPARE(drop(QStringLiteral("root dashboard")), QString());
        f.window->toggleCurrentLock();
        QCOMPARE(drop(QStringLiteral("root dashboard, lock toggled")), QString());

        // The item alone in a tab, with a crop.
        f.window->openItem(kImageTask, kGardenSketch);
        f.crop = QRect(100, 50, 40, 30);
        QCOMPARE(drop(QStringLiteral("item alone")), QString());
        const QImage saved(f.dataStore.filePath(f.file()));
        QCOMPARE(saved.size(), QSize(40, 30));
        QCOMPARE(saved.pixelColor(0, 0), QColor(Qt::green));
        QVERIFY(QFileInfo::exists(files.filePath(QStringLiteral("dropped.png"))));
    }

    // A cell of an item that is not an image refuses the drop, as the gaps
    // of the dashboard do. An image cell refuses a drag without an image.
    void cellRefusesOthers()
    {
        Fixture f;
        QVERIFY(f.writable());
        f.window->openTaskPreview(kImageTask, kOverviewDashboard);
        LayoutView *view = f.window->currentLayoutView();
        CellFrame *notes = itemCell(view, kNotes);
        QVERIFY(notes);
        QVERIFY(!notes->acceptDrops());
        const Task before = *f.model.task(kImageTask);
        const auto data = dataDrag();
        QCOMPARE(dragAndDrop(notes, data.get()), Qt::IgnoreAction);
        QCOMPARE(dragAndDrop(view, data.get()), Qt::IgnoreAction);

        f.window->openItem(kImageTask, kGardenSketch);
        CellFrame *image = itemCell(f.window->currentLayoutView(), kGardenSketch);
        QVERIFY(image && image->acceptDrops());
        QMimeData text;
        text.setText(QStringLiteral("not an image"));
        QCOMPARE(dragAndDrop(image, &text), Qt::IgnoreAction);

        QCoreApplication::processEvents();
        QVERIFY(f.dialogs.isEmpty());
        QVERIFY(*f.model.task(kImageTask) == before);
    }

    // In read-only data a drop on an Image cell says why in the status bar
    // and opens no dialog, as Ctrl+V does.
    void cellReadOnly()
    {
        Fixture f;
        QVERIFY(f.dataStore.readOnly());
        f.window->openItem(kImageTask, kGardenSketch);
        const QString before = f.file();
        CellFrame *image = itemCell(f.window->currentLayoutView(), kGardenSketch);
        QVERIFY(image);
        const auto data = dataDrag();
        QCOMPARE(dragAndDrop(image, data.get()), Qt::CopyAction);
        QTRY_COMPARE(f.window->statusBar()->currentMessage(), ImagePaste::message(ImagePaste::Result::SaveFailed));
        QVERIFY(f.dialogs.isEmpty());
        QCOMPARE(f.file(), before);
    }

    // While a dashboard is edited, the layout editor still takes a drag of
    // an item over an Image cell, which now takes image drops itself.
    void layoutEditorDragOverImageCell()
    {
        Fixture f;
        RootDashboard root;
        root.id = QStringLiteral("00000000-0000-0000-0000-0000000000d2");
        root.name = QStringLiteral("Drops");
        root.layout.ref = CellRef{kGardenSketch, kImageTask, QString()};
        f.model.addRootDashboard(root);
        f.window->openRootDashboard(root.id);
        if (!f.window->currentLayoutView()->editMode())
            f.window->toggleCurrentLock();
        QVERIFY(f.window->currentLayoutView()->editMode());
        CellFrame *image = itemCell(f.window->currentLayoutView(), kGardenSketch);
        QVERIFY(image && image->acceptDrops());

        DragPayload payload;
        payload.ref = CellRef{kNotes, kImageTask, QString()};
        std::unique_ptr<QMimeData> mime(dragMimeData(payload));
        const QPoint pos = image->rect().center();
        QDragEnterEvent enter(pos, Qt::CopyAction | Qt::MoveAction, mime.get(), Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(image, &enter);
        QVERIFY(enter.isAccepted());
        QDragMoveEvent move(pos, Qt::CopyAction | Qt::MoveAction, mime.get(), Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(image, &move);
        QVERIFY(move.isAccepted());
        QDragLeaveEvent leave;
        QApplication::sendEvent(image, &leave);
        QVERIFY(f.dialogs.isEmpty());
    }
};

QTEST_MAIN(TestImageDrops)
#include "tst_image_drops.moc"
