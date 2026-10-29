#pragma once

#include "core/LayoutTree.h"
#include "core/Model.h"

#include <QString>

#include <memory>
#include <optional>

class QImage;

namespace mirante {

class ItemTypeRegistry;
class LockState;
class Store;

// Pasting an image from the clipboard onto a dashboard or into an item. See
// docs/decisions/0021-image-items-as-files.md and the M3 plan, part 7.
// MainWindow calls it on Ctrl+V when the clipboard holds an image.
// Implemented in ImagePaste.cpp.
class ImagePaste
{
public:
    ImagePaste(Model &model, Store &store, const ItemTypeRegistry &types, const LockState &locks);
    ~ImagePaste();
    ImagePaste(const ImagePaste &) = delete;
    ImagePaste &operator=(const ImagePaste &) = delete;

    enum class Result {
        ReplacedImage,      // the focused item's image was replaced
        CreatedItem,        // a new item holds the image
        Locked,             // a new item was needed, but the dashboard is locked
        RootDashboard,      // a new item was needed on a root dashboard, and no task was named
        NoImageType,        // no registered type has a pastedImageField
        SaveFailed,         // the image could not be saved, for example read-only data
    };

    // Pastes onto the dashboard in front. focusedCell is the path of the
    // focused cell in it, if any. If that cell holds an item whose type has a
    // pastedImageField, its image is replaced, even when locked. Otherwise, on
    // an unlocked dashboard, a new item of the first such type is made, named
    // "Image" and numbered if needed: an empty focused cell holds it, any
    // other focused cell is split to the right. Without a focused cell, a
    // layout of one empty cell holds it, and any other layout's root is split.
    // The item belongs to the task dashboard's task. On a root dashboard it
    // belongs to rootTaskId; if that is empty, the result is RootDashboard, so
    // the caller can ask for a task and paste again.
    Result paste(const QImage &image, const DashboardRef &dashboard, const std::optional<NodePath> &focusedCell,
                 const QString &rootTaskId = QString());

    // Pastes into an item shown alone in a tab: replaces its image if its type
    // has a pastedImageField; otherwise returns NoImageType.
    Result pasteIntoItem(const QImage &image, const QString &taskId, const QString &itemId);

    // A short message for the status bar, or empty for success.
    static QString message(Result result);

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
