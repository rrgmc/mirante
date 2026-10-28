// Stub from task T0. Task T13 implements this file.
#include "layout/LayoutEditor.h"

#include "layout/LayoutView.h"

namespace mirante {

struct LayoutEditor::Private
{
};

LayoutEditor::LayoutEditor(Model & /*model*/, const ItemTypeRegistry & /*types*/, QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
}

LayoutEditor::~LayoutEditor() = default;

void LayoutEditor::attach(LayoutView * /*view*/, const DashboardRef & /*dashboard*/) {}
void LayoutEditor::detach(LayoutView * /*view*/) {}

void LayoutEditor::cellCommand(const DashboardRef & /*dashboard*/, const NodePath & /*path*/,
                               CellCommand /*command*/)
{
}

void LayoutEditor::changeItem(const DashboardRef & /*dashboard*/, const NodePath & /*path*/,
                              const CellRef & /*ref*/)
{
}

void LayoutEditor::tabCommand(const DashboardRef & /*dashboard*/, const NodePath & /*tabsPath*/, int /*index*/,
                              TabCommand /*command*/)
{
}

void LayoutEditor::renameTab(const DashboardRef & /*dashboard*/, const NodePath & /*tabsPath*/, int /*index*/,
                             const QString & /*title*/)
{
}

bool LayoutEditor::drop(const DashboardRef & /*dashboard*/, const DropTarget & /*target*/,
                        const DragPayload & /*payload*/)
{
    return false;
}

} // namespace mirante
