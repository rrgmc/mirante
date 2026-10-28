#pragma once

#include <QString>

// Code that differs between Windows, macOS and Linux lives in this folder.
// In M2 it only links qtkeychain and QHotkey. TerminalLauncher, GlobalHotkey,
// Tray and GitHead arrive with their milestones.

namespace mirante::platform {

// "windows", "macos" or "linux".
QString systemName();

} // namespace mirante::platform
