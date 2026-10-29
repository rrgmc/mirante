#pragma once

#include "core/LayoutTree.h"

#include <QString>
#include <QStringList>

// The starting layouts of docs/ui.md#layout-presets, offered when a dashboard
// is created. Implemented in Presets.cpp.

namespace mirante::presets {

// The preset names, in the order of docs/ui.md#layout-presets, translated.
QStringList names();

// The tree of preset index for a task dashboard. Cells are filled with
// itemIds in order, and the rest are empty. The Tabs preset has one tab per
// item, titled with itemNames, or one empty tab if there are no items.
LayoutNode build(int index, const QStringList &itemIds = {}, const QStringList &itemNames = {});

} // namespace mirante::presets
