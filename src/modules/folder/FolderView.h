#pragma once

class QWidget;

namespace mirante {
class ItemContext;
}

namespace mirante::modules::folder {

// The view of a Folder item: its local path, Git branch and status, and
// buttons for a terminal, an editor and the file manager. See
// docs/item-types.md#folder--terminal.
QWidget *createFolderView(ItemContext &context, QWidget *parent);

} // namespace mirante::modules::folder
