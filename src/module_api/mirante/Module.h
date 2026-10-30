#pragma once

#include <mirante/ApiClient.h>
#include <mirante/Fields.h>
#include <mirante/ItemType.h>
#include <mirante/ModuleSettings.h>

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

    // The module's name for the user, such as "GitLab". Each profile's
    // Modules submenu names the module's submenu with it. Defaults to the
    // module id.
    virtual void setModuleName(const QString &name) = 0;

    // One or two plain sentences that say what the module adds, such as
    // "Merge requests, pipelines and issues from a GitLab server." Help >
    // Modules shows it under the module's name. Defaults to none.
    virtual void setModuleDescription(const QString &description) = 0;

    // Takes ownership of a configuration screen. Its id must be lowercase
    // letters and digits, unique within the module, and its name not empty.
    virtual void addSettingsScreen(std::unique_ptr<SettingsScreen> screen) = 0;

    // The stored values of this module's screens, in each profile. It outlives the module's
    // item types, so they may keep a pointer to it, for example to offer
    // configured values in their fields().
    virtual const ModuleSettings &settings() const = 0;

    // Takes ownership of the description of the module's server: the screen
    // and token field that hold it, and how requests are authenticated. A
    // module has at most one. Item types reach the server through
    // ItemContext::server() and StatusJob::server(). See
    // docs/modules.md#servers-and-tokens.
    virtual void setServerType(std::unique_ptr<ServerType> type) = 0;

    // A field of the project dialog's section for this module, such as
    // GitLab's project path. Its value is kept in the project's
    // moduleSettings.<module id>.<key>, and items read it with
    // ItemContext::projectSetting(). Fields of the types Secret, Folder and
    // Image are refused. See docs/data-model.md#project.
    virtual void addProjectSetting(const Field &field) = 0;
};

} // namespace mirante
