// Stub from task T0. Task T10 implements this file.
#include "layout/LayoutView.h"

namespace mirante {

struct LayoutView::Private
{
    LayoutNode tree;
    bool editMode = false;
};

LayoutView::LayoutView(CellFactory * /*factory*/, QWidget *parent)
    : QWidget(parent)
    , d(std::make_unique<Private>())
{
}

LayoutView::~LayoutView() = default;

void LayoutView::setTree(const LayoutNode &tree) { d->tree = tree; }
const LayoutNode &LayoutView::tree() const { return d->tree; }

void LayoutView::setEditMode(bool on) { d->editMode = on; }
bool LayoutView::editMode() const { return d->editMode; }

void LayoutView::setSingleViewTracker(SingleViewTracker * /*tracker*/) {}

QWidget *LayoutView::cellAt(const NodePath & /*path*/) const { return nullptr; }
std::optional<NodePath> LayoutView::pathOf(const QWidget * /*cell*/) const { return std::nullopt; }
std::vector<QWidget *> LayoutView::visibleCells() const { return {}; }

QWidget *LayoutView::detachCell(const NodePath & /*path*/) { return nullptr; }
void LayoutView::reattachCell(QWidget * /*cell*/) {}

} // namespace mirante
