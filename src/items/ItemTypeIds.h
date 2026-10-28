#pragma once

#include <QString>

// The id rules from docs/modules.md#ids, used by ItemTypeRegistry.

namespace mirante {

// True if the module id has lowercase ASCII letters and digits only.
bool isValidModuleId(const QString &moduleId);

// True if the id equals the module id, or is the module id, a dot and a
// non-empty rest with no empty parts, such as "gitlab.mr.summary".
bool idBelongsToModule(const QString &id, const QString &moduleId);

} // namespace mirante
