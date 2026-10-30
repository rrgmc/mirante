#pragma once

#include "core/LayoutTree.h"
#include "core/Model.h"

#include <QString>

#include <memory>
#include <optional>

class QImage;
class QMimeData;

namespace mirante {

class ItemTypeRegistry;
class Store;

// Pasting an image from the clipboard into an item, or as a new item of a
// task. A pasted image never goes onto a dashboard: "Paste Image…" in a task's
// menus adds an item to the task, and Ctrl+V only replaces the image of a
// focused image item. See docs/decisions/0021-image-items-as-files.md and
// docs/decisions/0051-paste-images-from-the-items-menu.md.
// Implemented in ImagePaste.cpp.
class ImagePaste
{
public:
    ImagePaste(Model &model, Store &store, const ItemTypeRegistry &types);
    ~ImagePaste();
    ImagePaste(const ImagePaste &) = delete;
    ImagePaste &operator=(const ImagePaste &) = delete;

    enum class Result {
        ReplacedImage,      // the focused item's image was replaced
        CreatedItem,        // a new item of the task holds the image
        NoImageItem,        // Ctrl+V on a dashboard without a focused image item
        NoImageType,        // no registered type has a pastedImageField
        SaveFailed,         // the image could not be saved, for example read-only data
    };

    // Adds a new item to the task, of the first type with a pastedImageField,
    // named "Image" and numbered if needed. No dashboard changes: the user
    // drags the item onto one if wanted. On success, itemId, if given, gets
    // the new item's id.
    Result addToTask(const QImage &image, const QString &taskId, QString *itemId = nullptr);

    // Ctrl+V on the dashboard in front. focusedCell is the path of the focused
    // cell in it, if any. If that cell holds an item whose type has a
    // pastedImageField, its image is replaced, even when locked. Otherwise
    // nothing changes, and the result is NoImageItem, or NoImageType in a
    // build without an image type.
    Result paste(const QImage &image, const DashboardRef &dashboard, const std::optional<NodePath> &focusedCell);

    // Pastes into an item shown alone in a tab: replaces its image if its type
    // has a pastedImageField; otherwise returns NoImageType.
    Result pasteIntoItem(const QImage &image, const QString &taskId, const QString &itemId);

    // What addToTask(), paste() and pasteIntoItem() would return, without
    // saving or changing anything. The caller asks before it opens the paste
    // dialog, so a paste that cannot happen shows its message at once.
    // SaveFailed means a read-only workspace, or a task that is gone; other
    // save failures show only when the paste tries to save.
    Result checkTask(const QString &taskId) const;
    Result check(const DashboardRef &dashboard, const std::optional<NodePath> &focusedCell) const;
    Result checkItem(const QString &taskId, const QString &itemId) const;

    // A short message for the status bar, or empty for success.
    static QString message(Result result);

    // The image a drag from another app carries: the first local file whose
    // suffix is png, jpg, jpeg, gif, bmp or webp and that QImageReader reads,
    // else the drag's image data. A null image if it carries neither. See
    // docs/decisions/0085-drop-images-from-other-apps.md.
    static QImage imageFromMime(const QMimeData *data);

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
