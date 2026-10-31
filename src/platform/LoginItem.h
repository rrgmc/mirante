#pragma once

#include <QString>

#include <functional>

namespace mirante::platform::loginitem {

// Starts Mirante when the user signs in, hidden in the tray. Windows uses the
// per-user Run key, macOS 13 and later SMAppService.mainAppService, and Linux
// an XDG autostart file. The state lives in the system, so a change made in
// System Settings, in the autostart folder or by the Windows setup shows here.
// See docs/decisions/0105-start-at-sign-in.md.

// False before macOS 13.
bool isSupported();

bool isEnabled();

// On failure returns false and says why in error.
bool setEnabled(bool enabled, QString *error);

// macOS: Mirante is registered, but the user must still allow it in System
// Settings > General > Login Items. False elsewhere.
bool needsApproval();

// Opens System Settings > General > Login Items. Does nothing outside macOS.
void openSystemSettings();

// Calls callback once Mirante has finished launching, with whether the system
// started it at sign-in. On macOS the answer comes from the launch event,
// after QApplication::exec() starts, because a login item gets no arguments.
// Elsewhere callback runs at once with false, and the sign-in entry passes
// --hidden instead.
void whenLaunched(std::function<void(bool atSignIn)> callback);

} // namespace mirante::platform::loginitem
