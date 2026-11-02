#pragma once

#include <mirante/Fields.h>

#include <mirante/Fields.h>

#include <QString>

// The id rules from docs/modules.md#ids, used by the item type registry and by the test registrar.

namespace mirante {

// True if the module id has lowercase ASCII letters and digits only.
bool isValidModuleId(const QString &moduleId);

// True if the id equals the module id, or is the module id, a dot and a
// non-empty rest with no empty parts, such as "gitlab.mr.summary".
bool idBelongsToModule(const QString &id, const QString &moduleId);

// True if project settings take a field of the type. A token stays in a settings
// screen, and a project field has no folders or images to choose from.
bool projectSettingTakes(Field::Type type);

// True if device settings take a field of the type. See decision 0122.
bool deviceSettingTakes(Field::Type type);

// True if project settings take a field of the type. A token stays in a settings
// screen, and a project field has no folders or images to choose from.
bool projectSettingTakes(Field::Type type);

// True if device settings take a field of the type. See decision 0122.
bool deviceSettingTakes(Field::Type type);

} // namespace mirante
