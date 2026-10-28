#pragma once

#include <mirante/ItemType.h>

#include <memory>

namespace mirante {

// What a module registers its parts with. See docs/modules.md#what-a-module-registers.
//
// Each module defines one function, which the generated enabled_modules.cpp calls:
//
//     namespace mirante::modules::<id> {
//     void registerModule(mirante::ModuleRegistrar &registrar);
//     }
//
// The class is abstract, so a module's library has no link dependency on core code.
class ModuleRegistrar
{
public:
    virtual ~ModuleRegistrar() = default;

    // Takes ownership of the type. Its id must equal the module id or start
    // with the module id and a dot, and must not be registered twice.
    virtual void addItemType(std::unique_ptr<ItemType> type) = 0;
};

} // namespace mirante
