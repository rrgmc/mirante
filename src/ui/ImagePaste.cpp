#include "ui/ImagePaste.h"

#include "core/Edits.h"
#include "core/Store.h"
#include "items/FieldValidation.h"
#include "items/ItemTypeRegistry.h"
#include "layout/LockState.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QImage>
#include <QSet>

#include <algorithm>

namespace mirante {

struct ImagePaste::Private
{
    Model &model;
    Store &store;
    const ItemTypeRegistry &types;
    const LockState &locks;

    // The first registered type that a pasted image can fill, or null.
    const ItemType *imageType() const
    {
        for (const ItemType *type : types.types()) {
            if (!type->pastedImageField().isEmpty())
                return type;
        }
        return nullptr;
    }

    // The image as PNG in files/: the file's name, or empty if it could not be saved.
    QString save(const QImage &image) const
    {
        QByteArray bytes;
        QBuffer buffer(&bytes);
        if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG"))
            return {};
        buffer.close();
        return store.saveFile(bytes, QStringLiteral("png"));
    }

    // Replaces the image of an item whose type has a pastedImageField.
    // Returns nullopt if the item does not exist or its type has none.
    std::optional<Result> replace(const QImage &image, const QString &taskId, const QString &itemId)
    {
        const Task *task = model.task(taskId);
        if (!task)
            return std::nullopt;
        const auto item = std::find_if(task->items.begin(), task->items.end(),
                                        [&itemId](const Item &i) { return i.id == itemId; });
        if (item == task->items.end())
            return std::nullopt;
        const QString field = types.type(item->type).pastedImageField();
        if (field.isEmpty())
            return std::nullopt;

        const QString name = save(image);
        if (name.isEmpty())
            return Result::SaveFailed;
        Task changed = *task;
        for (Item &i : changed.items) {
            if (i.id == itemId)
                i.config.insert(field, name);
        }
        model.setTask(changed);
        return Result::ReplacedImage;
    }
};

namespace {

// "Image", or "Image 2", "Image 3" and so on, whichever no item of the task has.
QString freeName(const Task &task)
{
    QSet<QString> used;
    for (const Item &item : task.items)
        used.insert(item.name);
    const QString base = QCoreApplication::translate("ImagePaste", "Image");
    if (!used.contains(base))
        return base;
    for (int n = 2;; ++n) {
        const QString name = base + QLatin1Char(' ') + QString::number(n);
        if (!used.contains(name))
            return name;
    }
}

} // namespace

ImagePaste::ImagePaste(Model &model, Store &store, const ItemTypeRegistry &types, const LockState &locks)
    : d(std::make_unique<Private>(Private{model, store, types, locks}))
{
}

ImagePaste::~ImagePaste() = default;

ImagePaste::Result ImagePaste::paste(const QImage &image, const DashboardRef &dashboard,
                                     const std::optional<NodePath> &focusedCell, const QString &rootTaskId)
{
    // The focused cell, if the path leads to a cell of the dashboard's layout.
    const LayoutNode *tree = d->model.layout(dashboard);
    const LayoutNode *cell = nullptr;
    if (tree && focusedCell) {
        cell = layout::nodeAt(*tree, *focusedCell);
        if (cell && cell->kind != LayoutNode::Kind::Cell)
            cell = nullptr;
    }

    // An item edit, so it works on a locked dashboard and on a root dashboard.
    if (cell && cell->ref && cell->ref->isItem()) {
        const QString taskId = dashboard.isRoot() ? cell->ref->task : dashboard.taskId;
        if (const std::optional<Result> result = d->replace(image, taskId, cell->ref->item))
            return *result;
    }

    const ItemType *type = d->imageType();
    if (!type)
        return Result::NoImageType;
    if (!d->locks.isUnlocked(dashboard.dashboardId))
        return Result::Locked;
    // A root dashboard has no task of its own, so the caller names one.
    const QString taskId = dashboard.isRoot() ? rootTaskId : dashboard.taskId;
    if (taskId.isEmpty())
        return Result::RootDashboard;

    const Task *task = d->model.task(taskId);
    if (!task || !tree)
        return Result::SaveFailed;      // the task or dashboard is gone; nothing to paste into

    const QString file = d->save(image);
    if (file.isEmpty())
        return Result::SaveFailed;

    Item item;
    item.id = edits::newId();
    item.type = type->id();
    item.name = freeName(*task);
    item.config = defaultConfig(type->fields());
    item.config.insert(type->pastedImageField(), file);

    // A root dashboard's item cell names the task, since it can show items of any task.
    CellRef ref;
    ref.item = item.id;
    if (dashboard.isRoot())
        ref.task = task->id;

    LayoutNode layout;
    if (cell && !cell->ref)
        layout = layout::setRef(*tree, *focusedCell, ref);
    else if (cell)
        layout = layout::splitCell(*tree, *focusedCell, Edge::Right, ref).tree;
    else if (tree->kind == LayoutNode::Kind::Cell && !tree->ref)
        layout = layout::setRef(*tree, NodePath{}, ref);   // a dashboard of one empty cell
    else
        layout = layout::splitCell(*tree, NodePath{}, Edge::Right, ref).tree;

    Task changed = edits::addItem(*task, item);
    if (dashboard.isRoot()) {
        // The item first, so the root dashboard's cell refers to something.
        d->model.setTask(changed);
        d->model.setLayout(dashboard, layout);
        return Result::CreatedItem;
    }
    for (Dashboard &board : changed.dashboards) {
        if (board.id == dashboard.dashboardId)
            board.layout = layout;
    }
    d->model.setTask(changed);
    return Result::CreatedItem;
}

ImagePaste::Result ImagePaste::pasteIntoItem(const QImage &image, const QString &taskId, const QString &itemId)
{
    if (const std::optional<Result> result = d->replace(image, taskId, itemId))
        return *result;
    return Result::NoImageType;
}

QString ImagePaste::message(Result result)
{
    switch (result) {
    case Result::ReplacedImage:
    case Result::CreatedItem:
        return {};
    case Result::Locked:
        return QCoreApplication::translate("ImagePaste", "Unlock the dashboard to paste an image into it.");
    case Result::RootDashboard:
        return QCoreApplication::translate("ImagePaste", "A pasted image belongs to a task. Add an active task first.");
    case Result::NoImageType:
        return QCoreApplication::translate("ImagePaste", "This build cannot show images.");
    case Result::SaveFailed:
        return QCoreApplication::translate("ImagePaste", "The image could not be saved.");
    }
    return {};
}

} // namespace mirante
