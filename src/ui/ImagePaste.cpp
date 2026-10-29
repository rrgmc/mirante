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

    // The pastedImageField of an item's type, or empty if the item does not
    // exist or its type has none.
    QString imageField(const QString &taskId, const QString &itemId) const
    {
        const Task *task = model.task(taskId);
        if (!task)
            return {};
        const auto item = std::find_if(task->items.begin(), task->items.end(),
                                        [&itemId](const Item &i) { return i.id == itemId; });
        if (item == task->items.end())
            return {};
        return types.type(item->type).pastedImageField();
    }

    // What a paste onto a dashboard does, worked out before anything is saved.
    struct Target
    {
        Result result = Result::NoImageType;
        const LayoutNode *tree = nullptr;   // the dashboard's layout
        const LayoutNode *cell = nullptr;   // the focused cell, if the path leads to one
        QString taskId;                     // the task of the item replaced or made
        QString itemId;                     // the item replaced, for ReplacedImage
    };

    // A read-only workspace, such as the sample data, cannot save the image.
    Target target(const DashboardRef &dashboard, const std::optional<NodePath> &focusedCell,
                  const QString &rootTaskId) const
    {
        Target t = place(dashboard, focusedCell, rootTaskId);
        if ((t.result == Result::ReplacedImage || t.result == Result::CreatedItem) && store.readOnly())
            t.result = Result::SaveFailed;
        return t;
    }

    Target place(const DashboardRef &dashboard, const std::optional<NodePath> &focusedCell,
                 const QString &rootTaskId) const
    {
        Target t;
        t.tree = model.layout(dashboard);
        if (t.tree && focusedCell) {
            t.cell = layout::nodeAt(*t.tree, *focusedCell);
            if (t.cell && t.cell->kind != LayoutNode::Kind::Cell)
                t.cell = nullptr;
        }

        // An item edit, so it works on a locked dashboard and on a root dashboard.
        if (t.cell && t.cell->ref && t.cell->ref->isItem()) {
            const QString taskId = dashboard.isRoot() ? t.cell->ref->task : dashboard.taskId;
            if (!imageField(taskId, t.cell->ref->item).isEmpty()) {
                t.result = Result::ReplacedImage;
                t.taskId = taskId;
                t.itemId = t.cell->ref->item;
                return t;
            }
        }

        if (!imageType())
            return t;   // NoImageType
        if (!locks.isUnlocked(dashboard.dashboardId)) {
            t.result = Result::Locked;
            return t;
        }
        // A root dashboard has no task of its own, so the caller names one.
        t.taskId = dashboard.isRoot() ? rootTaskId : dashboard.taskId;
        if (t.taskId.isEmpty())
            t.result = Result::RootDashboard;
        else if (!model.task(t.taskId) || !t.tree)
            t.result = Result::SaveFailed;      // the task or dashboard is gone; nothing to paste into
        else
            t.result = Result::CreatedItem;
        return t;
    }

    // Replaces the image of an item whose type has a pastedImageField.
    // Returns nullopt if the item does not exist or its type has none.
    std::optional<Result> replace(const QImage &image, const QString &taskId, const QString &itemId)
    {
        const QString field = imageField(taskId, itemId);
        if (field.isEmpty())
            return std::nullopt;

        const QString name = save(image);
        if (name.isEmpty())
            return Result::SaveFailed;
        Task changed = *model.task(taskId);
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
    const Private::Target target = d->target(dashboard, focusedCell, rootTaskId);
    if (target.result == Result::ReplacedImage)
        return d->replace(image, target.taskId, target.itemId).value_or(Result::SaveFailed);
    if (target.result != Result::CreatedItem)
        return target.result;

    const LayoutNode *tree = target.tree;
    const LayoutNode *cell = target.cell;
    const Task *task = d->model.task(target.taskId);
    const ItemType *type = d->imageType();

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

ImagePaste::Result ImagePaste::check(const DashboardRef &dashboard, const std::optional<NodePath> &focusedCell,
                                     const QString &rootTaskId) const
{
    return d->target(dashboard, focusedCell, rootTaskId).result;
}

ImagePaste::Result ImagePaste::checkItem(const QString &taskId, const QString &itemId) const
{
    if (d->imageField(taskId, itemId).isEmpty())
        return Result::NoImageType;
    return d->store.readOnly() ? Result::SaveFailed : Result::ReplacedImage;
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
