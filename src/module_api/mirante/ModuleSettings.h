#pragma once

#include <mirante/Fields.h>

#include <QJsonObject>
#include <QString>

#include <vector>

class QWidget;

namespace mirante {

class ItemEditor;

// One configuration screen of a module, such as GitLab's "General
// Configuration". The Modules menu lists it by name, in a submenu named after
// the module. Its values are one JSON object per profile, kept in the
// workspace and synced. Secrets, such as tokens, never go there. See
// docs/decisions/0044-module-settings-in-the-workspace.md and
// docs/decisions/0050-profiles-hold-logins-connections-and-settings.md.
//
// Like an item type, a screen lists its fields, and the core builds the form,
// or returns an editor of its own.
class SettingsScreen
{
public:
    virtual ~SettingsScreen() = default;

    // Stable, because it names the stored values: lowercase letters and
    // digits, such as "general". Unique within the module.
    virtual QString id() const = 0;

    // The menu entry and the dialog title, such as "General Configuration".
    virtual QString name() const = 0;

    // The fields of the form the core builds.
    virtual std::vector<Field> fields() const { return {}; }

    // An editor of the screen's own, or null to use a form built from
    // fields(). It is given the stored object with setConfig(), and its
    // config() is stored. Include <mirante/ItemEditor.h> to implement it.
    virtual ItemEditor *createEditor(QWidget *parent) const
    {
        Q_UNUSED(parent)
        return nullptr;
    }
};

// Reads the stored values of a module's own settings screens. The registrar
// gives one to each module; it lives as long as the module's item types, so
// they may keep it. See ModuleRegistrar::settings().
//
// Each profile has its own values. An item uses those of its profile:
// ItemContext::moduleSettings() reads them for a view, and
// ItemType::fieldsForProfile() is given the profile's id for a form.
class ModuleSettings
{
public:
    virtual ~ModuleSettings() = default;

    // The current values of one of the module's screens in a profile, read on
    // each call. Empty if nothing is stored, the profile is unknown, or no
    // workspace is open.
    virtual QJsonObject values(const QString &profileId, const QString &screenId) const = 0;
};

} // namespace mirante
