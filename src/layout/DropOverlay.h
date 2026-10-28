#pragma once

#include "core/LayoutTree.h"
#include "layout/DragMime.h"

#include <QWidget>

#include <memory>

namespace mirante {

class LayoutView;

// Where a drop lands: a node of the view's tree and a zone on it.
struct DropTarget
{
    NodePath path;
    DropZone zone;
};

// A transparent child of a LayoutView, shown while a drag is over it. It
// divides the cell under the cursor into five zones, and shows an insert marker
// over a tab bar. Implemented in DropOverlay.cpp.
class DropOverlay : public QWidget
{
    Q_OBJECT

public:
    explicit DropOverlay(LayoutView *view);
    ~DropOverlay() override;

signals:
    void dropped(const mirante::DropTarget &target, const mirante::DragPayload &payload);

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
