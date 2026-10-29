#pragma once

#include <mirante/ItemType.h>
#include <mirante/Module.h>

#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>
#include <vector>

namespace mirante {

// Owns the item types and settings screens that modules register. Implemented
// in ItemTypeRegistry.cpp.
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
    // moduleId and a dot, if an id is registered twice, or if a settings
    // screen has an invalid or repeated id or an empty name.
    void registerModule(const QString &moduleId, RegisterFunction fn);

    // The registered type, or an UnknownItemType made for that id. Never null.
    const ItemType &type(const QString &id) const;

    // True if a module registered the id.
    bool contains(const QString &id) const;

    // The registered types, sorted by name.
    std::vector<const ItemType *> types() const;

    // Every connection kind that a registered type lists in connectionKinds(),
    // once each, sorted. The group dialog offers these. See
    // docs/decisions/0038-read-connections-in-m4.md.
    QStringList connectionKinds() const;

    // Module settings. See docs/decisions/0044-module-settings-in-the-workspace.md.

    // Reads the stored values of one screen, such as Model::moduleSettings().
    // Without a reader, every module's ModuleSettings returns empty objects.
    // It may be set after the modules registered.
    using SettingsReader = std::function<QJsonObject(const QString &moduleId, const QString &screenId)>;
    void setSettingsReader(SettingsReader reader);

    // The ids of the registered modules, sorted.
    QStringList moduleIds() const;
    // The name the module gave itself, else its id.
    QString moduleName(const QString &moduleId) const;
    // The module's settings screens, in the order it added them. Empty for an
    // unknown module.
    std::vector<const SettingsScreen *> settingsScreens(const QString &moduleId) const;
    // One screen, or null.
    const SettingsScreen *settingsScreen(const QString &moduleId, const QString &screenId) const;
    // The ModuleSettings the module was given, or null for an unknown module.
    const ModuleSettings *moduleSettings(const QString &moduleId) const;

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
