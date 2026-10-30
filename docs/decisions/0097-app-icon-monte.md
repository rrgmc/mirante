# 0097. The app icon is an M drawn as two peaks under the sun

- Status: Accepted
- Date: 2026-10-30

## Context

[Decision 0096](0096-app-icon-arcada.md) made the app icon "Arcada": a landscape seen through three arches. In the Windows tray, at 16 to 20 pixels, its pale sky, hills and sun blurred into one light shape, so the icon was hard to recognise.

## Decision

The icon is "Monte": the letter M drawn as two white peaks, with an amber sun above the valley between them. A rolling ground in light blue rises across the bottom, taken from the "Luneta" concept. The M is the app's initial and the view from a high place.

- Rounded square with a blue gradient from `#3b82f6` to `#1d4ed8`. The M is white, the sun `#fbbf24`, the ground `#60a5fa`.
- The files are the same as in 0092 and 0096: `resources/icons/mirante.svg` is the source, `resources/app/mirante.ico` is for `mirante.exe`, and `resources/app/mirante.icns` is for the macOS bundle, drawn on the macOS icon grid.
- The macOS menu bar icon, `resources/icons/mirante-tray.svg`, shows the M and the sun in full colour and the edge of the ground as a lighter line.

## Alternatives

- **A simpler Arcada for small sizes only.** It would keep Arcada, but needs a second colour drawing for the tray.
- **Monte without the ground.** It reads as well at small sizes. The ground adds a sense of landscape at larger sizes.

## Consequences

- The thick white M stays readable from 16 pixels up, on dark and light taskbars.
- The ground is light enough to show as its own band at larger sizes. Below about 24 pixels it only tints the bottom of the icon.
- A change to the drawing still needs new copies of the `.ico`, the `.icns` and the menu bar icon.
