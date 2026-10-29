#include "ui/ItemDialog.h"

#include "items/ItemTypeRegistry.h"

// Stubs written by T0 of the M3 plan. T9 implements them.

namespace mirante {

struct ItemDialog::Private
{
    Item item;
};

ItemDialog::ItemDialog(const Item &item, bool, const ItemTypeRegistry &, EditorContext, QWidget *parent)
    : QDialog(parent)
    , d(std::make_unique<Private>())
{
    d->item = item;
}

ItemDialog::~ItemDialog() = default;

Item ItemDialog::value() const { return d->item; }
void ItemDialog::setType(const QString &typeId) { d->item.type = typeId; }
void ItemDialog::setName(const QString &name) { d->item.name = name; }
ItemEditor *ItemDialog::editor() const { return nullptr; }
void ItemDialog::accept() { QDialog::accept(); }

} // namespace mirante
