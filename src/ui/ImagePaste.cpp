#include "ui/ImagePaste.h"

#include "core/Edits.h"
#include "core/Store.h"
#include "items/FieldValidation.h"
#include "items/ItemTypeRegistry.h"

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

    // The image item in the focused cell of a dashboard, as its task and
    // item ids, or nullopt if the cell holds no item whose type has a
    // pastedImageField.
    std::optional<std::pair<QString, QString>> focusedImageItem(const DashboardRef &dashboard,
                                                                const std::optional<NodePath> &focusedCell) const
    {
        const LayoutNode *tree = model.layout(dashboard);
        if (!tree || !focusedCell)
            return std::nullopt;
        const LayoutNode *cell = layout::nodeAt(*tree, *focusedCell);
        if (!cell || cell->kind != LayoutNode::Kind::Cell || !cell->ref || !cell->ref->isItem())
            return std::nullopt;
        // A root dashboard's item cell names its task.
        const QString taskId = dashboard.isRoot() ? cell->ref->task : dashboard.taskId;
        if (imageField(taskId, cell->ref->item).isEmpty())
            return std::nullopt;
        return std::make_pair(taskId, cell->ref->item);
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

ImagePaste::ImagePaste(Model &model, Store &store, const ItemTypeRegistry &types)
    : d(std::make_unique<Private>(Private{model, store, types}))
{
}

ImagePaste::~ImagePaste() = default;

ImagePaste::Result ImagePaste::addToTask(const QImage &image, const QString &taskId, QString *itemId)
{
    const Result checked = checkTask(taskId);
    if (checked != Result::CreatedItem)
        return checked;

    const ItemType *type = d->imageType();
    const QString file = d->save(image);
    if (file.isEmpty())
        return Result::SaveFailed;

    const Task *task = d->model.task(taskId);
    Item item;
    item.id = edits::newId();
    item.type = type->id();
    item.name = freeName(*task);
    item.config = defaultConfig(type->fields());
    item.config.insert(type->pastedImageField(), file);
    d->model.setTask(edits::addItem(*task, item));
    if (itemId)
        *itemId = item.id;
    return Result::CreatedItem;
}

ImagePaste::Result ImagePaste::paste(const QImage &image, const DashboardRef &dashboard,
                                     const std::optional<NodePath> &focusedCell)
{
    const Result checked = check(dashboard, focusedCell);
    if (checked != Result::ReplacedImage)
        return checked;
    const auto target = d->focusedImageItem(dashboard, focusedCell);
    return d->replace(image, target->first, target->second).value_or(Result::SaveFailed);
}

ImagePaste::Result ImagePaste::pasteIntoItem(const QImage &image, const QString &taskId, const QString &itemId)
{
    if (const std::optional<Result> result = d->replace(image, taskId, itemId))
        return *result;
    return Result::NoImageType;
}

ImagePaste::Result ImagePaste::checkTask(const QString &taskId) const
{
    if (!d->imageType())
        return Result::NoImageType;
    // A read-only workspace, such as the sample data, cannot save the image.
    if (d->store.readOnly() || !d->model.task(taskId))
        return Result::SaveFailed;
    return Result::CreatedItem;
}

ImagePaste::Result ImagePaste::check(const DashboardRef &dashboard, const std::optional<NodePath> &focusedCell) const
{
    if (!d->focusedImageItem(dashboard, focusedCell))
        return d->imageType() ? Result::NoImageItem : Result::NoImageType;
    return d->store.readOnly() ? Result::SaveFailed : Result::ReplacedImage;
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
    case Result::NoImageItem:
        return QCoreApplication::translate("ImagePaste",
                                           "To add an image, use Paste Image… in a task's Items menu.");
    case Result::NoImageType:
        return QCoreApplication::translate("ImagePaste", "This build cannot show images.");
    case Result::SaveFailed:
        return QCoreApplication::translate("ImagePaste", "The image could not be saved.");
    }
    return {};
}

} // namespace mirante
