# 0107. Closing the window quits by default

- Status: Accepted
- Date: 2026-10-31

## Context

[Decision 0075](0075-hide-to-tray-and-show-hide-hotkey.md) made closing the main window hide it to the tray, with a device setting to turn this off. The user wants the opposite default: a close that quits, as in most desktop apps, and hiding to the tray only when chosen.

## Decision

- The device setting `window/closeToTray` is off when it is not set. Closing the main window then quits Mirante.
- With the setting on and a tray present, closing hides the app to the tray, as decision 0075 describes.
- The rest of decision 0075 stands. The tray icon, its menu and the global hotkey work with the setting off, and the hotkey still hides the app to the tray.

## Alternatives

- **Keep hiding on close by default.** The user asked for the change.
- **Remove the tray when the setting is off.** The tray also shows the attention count and the notifications, and the hotkey hides into it.

## Consequences

- A device that never changed the setting now quits on close after the update. The setting turns the old behaviour back on.
- Starting at sign-in ([decision 0106](0106-start-at-sign-in.md)) still starts hidden in the tray. The first close after showing the window quits, unless the setting is on.
