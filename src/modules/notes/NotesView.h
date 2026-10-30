#pragma once

class QWidget;

namespace mirante {
class ItemContext;
}

namespace mirante::modules::notes {

// The view of a Notes item: a Markdown editor and its preview. It saves the
// text into the configuration key "text" itself; see decision 0081 and
// docs/item-types.md#notes.
QWidget *createNotesView(ItemContext &context, QWidget *parent);

} // namespace mirante::modules::notes
