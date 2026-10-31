#pragma once

#include <QString>

namespace mirante {
class ItemContext;
}

namespace mirante::modules::folder {

// Opens path in the editor device setting, a command line such as "code -n":
// its first part is the program, and the path its last argument. Starts it in
// dir. Returns empty on success, else a message for the user.
QString startEditor(ItemContext &context, const QString &dir, const QString &path);

} // namespace mirante::modules::folder
