#pragma once

#include <QtGlobal>

namespace mirante::platform {

// Lets another process bring its window to the front. Windows keeps a
// background process from taking the foreground, so a later start of Mirante
// calls this for the running one before it hands over. With pid 0, any
// process may. On macOS and Linux nothing happens: the running instance's
// own raise and activate are enough there. See
// docs/decisions/0058-one-instance-per-web-data-folder.md.
void allowForegroundFor(qint64 pid);

} // namespace mirante::platform
