# 0076. The default global hotkey is pressed with the left hand

- Status: Accepted
- Date: 2026-10-30

## Context

[Decision 0075](0075-hide-to-tray-and-show-hide-hotkey.md) made the default Alt+Shift+M, or Control+Option+M on macOS. M is under the right hand, so the keys take two hands, or a hand off the mouse.

## Decision

The default is Alt+Shift+D, or Control+Shift+D on macOS. The user can still change or clear it in the settings window.

On macOS this is the physical Control key, which Qt calls Meta. Qt's Ctrl is Command there, and Command+Shift+D is New Dashboard.

## Alternatives

- **Keep the keys of favdir, with another letter.** D is next to F on the keyboard, and only macOS needs other modifiers, so the two apps still sit together.
- **Option+Shift+D on macOS, as on Windows.** macOS 15 refuses hotkeys that use only Option and Shift.

## Consequences

- A device that saved a hotkey in the settings window keeps it. The new default applies only where none is saved.
- Another application that holds these keys makes the registration fail, and the status bar and settings window say so.
