// Stub from task T0. Task T12 implements this file.
#include "ui/QuickSwitcher.h"

namespace mirante {

struct QuickSwitcher::Private
{
};

QuickSwitcher::QuickSwitcher(const Model & /*model*/, QWidget *parent)
    : QFrame(parent)
    , d(std::make_unique<Private>())
{
    hide();
}

QuickSwitcher::~QuickSwitcher() = default;

void QuickSwitcher::popup() {}

std::vector<SwitcherEntry> QuickSwitcher::candidates(const Model & /*model*/) { return {}; }

std::vector<SwitcherEntry> QuickSwitcher::rank(const std::vector<SwitcherEntry> & /*entries*/,
                                               const QString & /*query*/)
{
    return {};
}

} // namespace mirante
