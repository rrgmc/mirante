#include "items/ItemTypeRegistry.h"

#include "items/ItemTypeIds.h"
#include "items/UnknownItemType.h"

#include <QtGlobal>

#include <algorithm>
#include <map>

namespace mirante {

struct ItemTypeRegistry::Private
{
    std::vector<std::unique_ptr<ItemType>> types;
    std::map<QString, const ItemType *> byId;
    // Made on first use by type(), so callers always get a type back.
    mutable std::map<QString, std::unique_ptr<UnknownItemType>> unknown;

    void add(const QString &moduleId, std::unique_ptr<ItemType> type);
};

void ItemTypeRegistry::Private::add(const QString &moduleId, std::unique_ptr<ItemType> type)
{
    if (!type)
        qFatal("Module %s registered a null item type", qUtf8Printable(moduleId));
    const QString id = type->id();
    if (!idBelongsToModule(id, moduleId))
        qFatal("Module %s registered the item type \"%s\", which does not start with \"%s.\"",
               qUtf8Printable(moduleId), qUtf8Printable(id), qUtf8Printable(moduleId));
    if (byId.count(id) != 0)
        qFatal("Module %s registered the item type \"%s\" twice", qUtf8Printable(moduleId),
               qUtf8Printable(id));
    byId.emplace(id, type.get());
    types.push_back(std::move(type));
}

ItemTypeRegistry::ItemTypeRegistry()
    : d(std::make_unique<Private>())
{
}

ItemTypeRegistry::~ItemTypeRegistry() = default;

void ItemTypeRegistry::registerModule(const QString &moduleId, RegisterFunction fn)
{
    if (!isValidModuleId(moduleId))
        qFatal("Invalid module id \"%s\": use lowercase letters and digits only",
               qUtf8Printable(moduleId));
    if (!fn)
        qFatal("Module %s has no register function", qUtf8Printable(moduleId));
    // The registrar a module's register function sees. It knows the module id,
    // so it can check every type id against it.
    class Registrar : public ModuleRegistrar
    {
    public:
        Registrar(Private &registry, const QString &id)
            : m_registry(registry)
            , m_moduleId(id)
        {
        }

        void addItemType(std::unique_ptr<ItemType> type) override
        {
            m_registry.add(m_moduleId, std::move(type));
        }

    private:
        Private &m_registry;
        QString m_moduleId;
    };

    Registrar registrar(*d, moduleId);
    fn(registrar);
}

const ItemType &ItemTypeRegistry::type(const QString &id) const
{
    const auto found = d->byId.find(id);
    if (found != d->byId.end())
        return *found->second;
    auto &slot = d->unknown[id];
    if (!slot)
        slot = std::make_unique<UnknownItemType>(id);
    return *slot;
}

bool ItemTypeRegistry::contains(const QString &id) const
{
    return d->byId.count(id) != 0;
}

std::vector<const ItemType *> ItemTypeRegistry::types() const
{
    std::vector<const ItemType *> result;
    result.reserve(d->types.size());
    for (const auto &type : d->types)
        result.push_back(type.get());
    std::sort(result.begin(), result.end(), [](const ItemType *a, const ItemType *b) {
        const int byName = a->name().compare(b->name(), Qt::CaseInsensitive);
        if (byName != 0)
            return byName < 0;
        return a->id() < b->id();
    });
    return result;
}

} // namespace mirante
