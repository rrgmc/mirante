// Stub from task T0. Task T10 implements this file.
#include "layout/CellFrame.h"

namespace mirante {

struct CellFrame::Private
{
    std::optional<CellRef> ref;
};

CellFrame::CellFrame(const std::optional<CellRef> &ref, bool /*removed*/, const ItemType * /*type*/,
                     const QString & /*itemName*/, QWidget *parent)
    : QFrame(parent)
    , d(std::make_unique<Private>())
{
    d->ref = ref;
}

CellFrame::~CellFrame() = default;

std::optional<CellRef> CellFrame::ref() const { return d->ref; }
void CellFrame::setEditMode(bool /*on*/) {}
void CellFrame::setCellFocused(bool /*focused*/) {}
bool CellFrame::isCellFocused() const { return false; }

} // namespace mirante
