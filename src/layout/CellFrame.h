#pragma once

#include "core/LayoutTree.h"

#include <QFrame>

#include <memory>
#include <optional>

namespace mirante {

class ItemType;

// A cell of an item: a header with a pill for the type, the item name, a pill
// for where the item comes from and the menu button, and a body. In M2 the body is a placeholder. See the M2 plan, part 4.
// Implemented in CellFrame.cpp.
class CellFrame : public QFrame
{
    Q_OBJECT

public:
    // type is null for an empty cell. removed is set for a cell whose
    // reference was deleted.
    CellFrame(const std::optional<CellRef> &ref, bool removed, const ItemType *type, const QString &itemName,
              QWidget *parent = nullptr);
    ~CellFrame() override;

    std::optional<CellRef> ref() const;

    // A grey pill after the item name that says where the item comes from,
    // such as "backend › Overview". Used for items placed directly in a root
    // dashboard. Empty hides it.
    void setContext(const QString &text);

    // In edit mode the header is a drag handle.
    void setEditMode(bool on);

    // A focused cell has a highlighted border.
    void setCellFocused(bool focused);
    bool isCellFocused() const;

    // Replaces the text in the body, such as "Removed", with another one,
    // such as "In trash".
    void setMessage(const QString &text);

signals:
    void menuRequested(const QPoint &globalPos);
    void dragStarted();     // the header was dragged in edit mode

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
