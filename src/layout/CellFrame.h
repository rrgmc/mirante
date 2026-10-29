#pragma once

#include "core/LayoutTree.h"

#include <QFrame>

#include <memory>
#include <optional>

namespace mirante {

class ItemType;

// A cell of an item: a header with a pill for the type, the item name, a pill
// for where the item comes from, a widget of the view such as a web toolbar,
// the menu button and the maximise button, and a body. The body is a
// placeholder until setBody() gives it a Placement. See the M2 plan, part 4,
// and the M4 plan, parts 5 and 6. Implemented in CellFrame.cpp.
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

    // Replaces the placeholder body with a widget, such as a Placement. The
    // frame takes ownership.
    void setBody(QWidget *body);
    QWidget *body() const;

    // A widget shown in the header between the item name and the menu
    // button, such as a web view's toolbar. Null removes it. The frame does
    // not take ownership.
    void setHeaderWidget(QWidget *widget);

    // Shows the maximise button, which emits maximiseRequested. Off for empty
    // and removed cells.
    void setMaximiseButtonVisible(bool visible);
    // Switches the button between "Maximise" and "Restore".
    void setMaximised(bool maximised);

signals:
    void menuRequested(const QPoint &globalPos);
    void dragStarted();     // the header was dragged in edit mode
    void maximiseRequested();

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
