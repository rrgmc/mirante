#pragma once

#include <QString>

// Code that differs between Windows, macOS and Linux lives in this folder.
// TerminalLauncher and GitHead arrive with their milestones. AttachedDialogs.h
// keeps dialogs above their window on macOS. Foreground.h lets a later start
// bring the running instance to the front on Windows, and tells whether the app
// is in front. GlobalHotkey.h and Tray.h let the window hide and come back.

namespace mirante::platform {

// "windows", "macos" or "linux".
QString systemName();

} // namespace mirante::platform
