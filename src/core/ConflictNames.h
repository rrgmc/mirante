#pragma once

#include <QString>

// Recognises the conflict copies that sync tools make. See
// docs/decisions/0031-conflict-copies-by-id-prefix.md.
// Implemented in ConflictNames.cpp.

namespace mirante::conflicts {

// For a file name in the data folder, without its folder, such as
// "<uuid>.sync-conflict-20261028-101500-ABCDEFG.json", the name of the file it
// is a conflict copy of, such as "<uuid>.json". An empty string if the name is
// not a conflict copy.
QString originalOf(const QString &fileName);

} // namespace mirante::conflicts
