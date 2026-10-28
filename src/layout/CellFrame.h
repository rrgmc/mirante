#pragma once

#include "core/LayoutTree.h"

#include <QFrame>

#include <memory>
#include <optional>

namespace mirante {

class ItemType;

// A cell of an item: a header with the type icon, the item name and the menu
// button, and a body. In M2 the body is a placeholder. See the M2 plan, part 4.
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

    // Dimmed text after the item name that says where the item comes from,
    // such as "backend / Overview". Used for items placed directly in a root
    // dashboard. Empty hides it.
    void setContext(const QString &text);

    // In edit mode the header is a drag handle.
    void setEditMode(bool on);

    // A focused cell has a highlighted border.
    void setCellFocused(bool focused);
    bool isCellFocused() const;

signals:
    void menuRequested(const QPoint &globalPos);
    void dragStarted();     // the header was dragged in edit mode

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
