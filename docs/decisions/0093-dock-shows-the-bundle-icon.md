# 0093. The macOS Dock shows the bundle's icon

- Status: Accepted
- Date: 2026-10-30

## Context

[Decision 0092](0092-app-icon-vista.md) draws the bundle icon, `mirante.icns`, on the macOS icon grid, so it is the size of other apps' icons. While the app ran, Qt replaced the Dock icon with the SVG, which has no grid margin. The running icon was therefore larger than the icon in the Finder and the icons of other apps.

## Decision

On macOS, Mirante does not set an icon at run time. `platform::setAppIcon()` sets the window icon on Windows and Linux and does nothing on macOS, so the Dock shows the bundle's icon file. macOS windows have no icon, so nothing else changes.

The menu bar code no longer sets the Dock icon again when the app returns to the Dock. That step only restored the icon Qt had set.

## Alternatives

- **Give the SVG a grid margin.** Windows and Linux would then show a smaller icon than other apps there.
- **Set a second SVG with the margin on macOS.** This keeps two drawings in step for no gain over the bundle icon.

## Consequences

- The Dock, the Finder and the app switcher show the same icon.
- `QApplication::windowIcon()` is empty on macOS. The menu bar uses its own monochrome icon, so it does not need it.
