# 0075. Closing hides to the tray, and a global hotkey shows or hides the app

- Status: Accepted; the default hotkey superseded by 0076
- Date: 2026-10-30

## Context

Mirante should be at hand all day without taking a place among the open windows. The user's other desktop app, favdir, already behaves this way, and Mirante should work the same.

The design said the hotkey only brings Mirante to the front. It also planned the tray and hotkey for M8, without saying what happens to detached windows, or how a real quit differs from a close.

## Decision

- Closing the main window hides it to the tray, and Mirante keeps running. A device setting turns this off. Without a tray, closing quits as before.
- A global hotkey, which the user can change or clear, shows or hides the app. If a window of the app is in front, the hotkey hides the app. Otherwise it brings the app to the front.
- The default hotkey is Alt+Shift+M, or Control+Option+M on macOS. favdir uses the same keys with F, so the two can run side by side.
- Hiding also hides the other open windows, such as detached dashboards and the settings window. They come back with the main window. Detached dashboards stay detached.
- An open modal dialog is never hidden. The hotkey and the tray bring it to the front instead, and the tray's Quit does not quit under it.
- Every quit goes through `QApplication::quit()`. The main window sees the quit event first, so File > Quit, the tray, macOS and the end of a Windows session all save the same way.
- The hotkey stays on QHotkey, as the architecture says, rather than favdir's own code. QHotkey also covers X11.
- On macOS the tray is a native status item, copied from favdir, because `QSystemTrayIcon` in Qt 6.8 aborts on macOS 27 when clicked. Mirante also leaves the Dock while hidden, as favdir does.

## Alternatives

- **Hide only the main window.** Detached windows would stay on screen, and the hotkey could not clear the app away.
- **Put detached windows back on hide.** The user would have to detach them again each time.
- **Copy favdir's hotkey backends.** This adds code for what the existing dependency already does.
- **Start at sign-in, hidden.** favdir has this too. It is a separate choice and is not part of this decision.

## Consequences

- A running Mirante now has no visible window most of the time, so a later start or the hotkey is how it comes back. Decision 0058 already covers a later start.
- Test runs beside the user's own Mirante cannot register the same hotkey. They say so in the status bar and the settings window, and otherwise work.
- The macOS code is built by CI but was not run during this work, which happened on Windows.
- The tray's attention count stays in M8.
