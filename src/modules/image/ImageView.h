#pragma once

class QWidget;

namespace mirante {
class ItemContext;
}

namespace mirante::modules::image {

// The view of an Image item: the image scaled to fit, the memo under it, and
// its link on a click. See docs/item-types.md#image.
QWidget *createImageView(ItemContext &context, QWidget *parent);

} // namespace mirante::modules::image
