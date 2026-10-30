#pragma once

#include <QString>
#include <QStringList>

#include <vector>

namespace mirante {

class ItemType;
class ModuleSettings;
class ServerType;
class SettingsScreen;

// What the api library needs to know about the registered modules. The
// ItemTypeRegistry, in the items library above, implements it, so this
// library does not depend on items. See docs/architecture.md#api.
class ModuleDirectory
{
public:
    virtual ~ModuleDirectory() = default;

    // The ids of the registered modules, sorted.
    virtual QStringList moduleIds() const = 0;
    // A registered type, or null.
    virtual const ItemType *findType(const QString &typeId) const = 0;
    // The module's server, or null if it registered none.
    virtual const ServerType *serverType(const QString &moduleId) const = 0;
    // The ModuleSettings the module was given, or null for an unknown module.
    virtual const ModuleSettings *moduleSettings(const QString &moduleId) const = 0;
    // The module's settings screens, in the order it added them.
    virtual std::vector<const SettingsScreen *> settingsScreens(const QString &moduleId) const = 0;
};

} // namespace mirante
