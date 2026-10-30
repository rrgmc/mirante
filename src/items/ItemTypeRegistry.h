#pragma once

#include "api/ModuleDirectory.h"

#include <mirante/ItemType.h>
#include <mirante/Module.h>

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QUuid>

#include <functional>
#include <memory>
#include <vector>

namespace mirante {

// Owns the item types and settings screens that modules register. Implemented
// in ItemTypeRegistry.cpp.
class ItemTypeRegistry : public ModuleDirectory
{
public:
    ItemTypeRegistry();
    ~ItemTypeRegistry() override;
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

    // Whether items of the type use their profile: its views are web views,
    // or its module has settings screens. The item
    // dialog offers a profile for such types. False for an unknown type.
    bool usesProfile(const QString &typeId) const;

    // Module settings. See decisions 0044 and 0050 in docs/decisions/.

    // Reads the stored values of one screen in a profile, such as
    // Model::moduleSettings().
    // Without a reader, every module's ModuleSettings returns empty objects.
    // It may be set after the modules registered.
    using SettingsReader = std::function<QJsonObject(const QString &profileId, const QString &moduleId, const QString &screenId)>;
    void setSettingsReader(SettingsReader reader);

    // The ids of the registered modules, sorted.
    QStringList moduleIds() const override;
    // The name the module gave itself, else its id.
    QString moduleName(const QString &moduleId) const;
    // The description the module gave itself, else empty.
    QString moduleDescription(const QString &moduleId) const;
    // The UUID, version, author and homepage the module gave itself, each
    // null or empty without one. See decision 0089.
    QUuid moduleUuid(const QString &moduleId) const;
    QString moduleVersion(const QString &moduleId) const;
    QString moduleAuthor(const QString &moduleId) const;
    QUrl moduleHomepage(const QString &moduleId) const;
    // The module's item types, sorted by name, as types() sorts them. Empty
    // for an unknown module.
    std::vector<const ItemType *> moduleTypes(const QString &moduleId) const;
    // The module's settings screens, in the order it added them. Empty for an
    // unknown module.
    std::vector<const SettingsScreen *> settingsScreens(const QString &moduleId) const override;
    // One screen, or null.
    const SettingsScreen *settingsScreen(const QString &moduleId, const QString &screenId) const;
    // The ModuleSettings the module was given, or null for an unknown module.
    const ModuleSettings *moduleSettings(const QString &moduleId) const override;

    // A registered type, or null. Unlike type(), never makes an UnknownItemType.
    const ItemType *findType(const QString &typeId) const override;
    // The module's server, or null if it registered none.
    const ServerType *serverType(const QString &moduleId) const override;
    // The module's project settings, in the order it added them. Empty for
    // an unknown module. See ModuleRegistrar::addProjectSetting().
    std::vector<Field> projectSettings(const QString &moduleId) const;
    // The ids of the modules with project settings, sorted.
    QStringList projectSettingModules() const;

    // Quick add. See docs/modules.md#quick-add and decision 0078.
    struct ModuleHandler
    {
        QString moduleId;
        const QuickAddHandler *handler = nullptr;
    };
    struct ModuleFinder
    {
        QString moduleId;
        const RelatedItemFinder *finder = nullptr;
    };
    // Every module's handlers and finders, by module id, then in the order
    // each module added them.
    std::vector<ModuleHandler> quickAddHandlers() const;
    std::vector<ModuleFinder> relatedItemFinders() const;

    // The module's device settings, in the order it added them. Empty for an
    // unknown module. See decision 0079.
    std::vector<Field> deviceSettings(const QString &moduleId) const;
    // The ids of the modules with device settings, sorted.
    QStringList deviceSettingModules() const;

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
