#pragma once

#include <mirante/ItemType.h>
#include <mirante/Module.h>

#include <QString>

#include <memory>
#include <vector>

namespace mirante {

// Owns the item types that modules register. Implemented in ItemTypeRegistry.cpp.
class ItemTypeRegistry
{
public:
    ItemTypeRegistry();
    ~ItemTypeRegistry();
    ItemTypeRegistry(const ItemTypeRegistry &) = delete;
    ItemTypeRegistry &operator=(const ItemTypeRegistry &) = delete;

    using RegisterFunction = void (*)(ModuleRegistrar &);

    // Calls fn with a registrar that knows moduleId. The registrar stops the
    // program with qFatal() if a type id does not equal moduleId or start with
    // moduleId and a dot, or if an id is registered twice.
    void registerModule(const QString &moduleId, RegisterFunction fn);

    // The registered type, or an UnknownItemType made for that id. Never null.
    const ItemType &type(const QString &id) const;

    // True if a module registered the id.
    bool contains(const QString &id) const;

    // The registered types, sorted by name.
    std::vector<const ItemType *> types() const;

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
