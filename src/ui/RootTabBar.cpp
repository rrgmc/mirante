// Stub from task T0. Task T15 implements this file.
#include "ui/RootTabBar.h"

namespace mirante {

struct RootTabBar::Private
{
    int previewIndex = -1;
};

RootTabBar::RootTabBar(QWidget *parent)
    : QTabBar(parent)
    , d(std::make_unique<Private>())
{
}

RootTabBar::~RootTabBar() = default;

void RootTabBar::setPreviewIndex(int index) { d->previewIndex = index; }
int RootTabBar::previewIndex() const { return d->previewIndex; }

void RootTabBar::paintEvent(QPaintEvent *event) { QTabBar::paintEvent(event); }

} // namespace mirante
