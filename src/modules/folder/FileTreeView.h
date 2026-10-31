#pragma once

class QWidget;

namespace mirante {
class ItemContext;
}

namespace mirante::modules::folder {

// The view of a File tree item: the files under one of the project's folders.
// See docs/item-types.md#file-tree.
QWidget *createFileTreeView(ItemContext &context, QWidget *parent);

} // namespace mirante::modules::folder
