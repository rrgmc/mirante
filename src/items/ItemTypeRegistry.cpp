// Stub from task T0. Task T2 implements this file.
#include "items/ItemTypeRegistry.h"

#include "items/UnknownItemType.h"

#include <map>

namespace mirante {

struct ItemTypeRegistry::Private
{
    mutable std::map<QString, std::unique_ptr<UnknownItemType>> unknown;
};

ItemTypeRegistry::ItemTypeRegistry()
    : d(std::make_unique<Private>())
{
}

ItemTypeRegistry::~ItemTypeRegistry() = default;

void ItemTypeRegistry::registerModule(const QString & /*moduleId*/, RegisterFunction /*fn*/) {}

const ItemType &ItemTypeRegistry::type(const QString &id) const
{
    auto &slot = d->unknown[id];
    if (!slot)
        slot = std::make_unique<UnknownItemType>(id);
    return *slot;
}

bool ItemTypeRegistry::contains(const QString & /*id*/) const { return false; }

std::vector<const ItemType *> ItemTypeRegistry::types() const { return {}; }

} // namespace mirante
