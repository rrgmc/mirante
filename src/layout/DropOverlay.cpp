// Stub from task T0. Task T13 implements this file.
#include "layout/DropOverlay.h"

#include "layout/LayoutView.h"

namespace mirante {

struct DropOverlay::Private
{
};

DropOverlay::DropOverlay(LayoutView *view)
    : QWidget(view)
    , d(std::make_unique<Private>())
{
    hide();
}

DropOverlay::~DropOverlay() = default;

} // namespace mirante
