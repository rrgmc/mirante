#pragma once

#include <QStringList>

// Implemented by enabled_modules.cpp, which CMake generates in the build folder
// from cmake/enabled_modules.cpp.in. See docs/modules.md#building.

namespace mirante {

class ItemTypeRegistry;

// Registers every module in this build, in alphabetical order of module id.
void registerEnabledModules(ItemTypeRegistry &registry);

// The ids of the modules in this build, sorted.
QStringList enabledModuleIds();

} // namespace mirante
