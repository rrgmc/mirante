#pragma once

class QWidget;

namespace mirante {
class ItemContext;
}

namespace mirante::modules::notes {

// The view of a Links item: a list of titles with short descriptions. A click
// or Return opens the link in the external program. See
// docs/item-types.md#links.
QWidget *createLinksView(ItemContext &context, QWidget *parent);

} // namespace mirante::modules::notes
