# 0091. The app icon is a sunrise over dashboard tiles

- Status: Accepted
- Date: 2026-10-30

## Context

The first app icon was three white panes on a blue square. It said "dashboard" but nothing about the name. [Decision 0014](0014-name-mirante.md) named the app Mirante, a lookout point from which you see everything around you at once. The app needs a lasting icon that carries that meaning and still reads at 16 pixels in the tray, where the attention badge covers its lower right two thirds.

## Decision

The icon, called "Vista", shows a sun rising on a horizon above four tiles drawn in perspective, as if seen from a high place. The tiles are both the landscape below the lookout and the cells of a dashboard.

- Rounded square in dark blue `#1e3a8a`, sun and horizon in amber `#fbbf24`, tiles in white. The far tiles are 75% opaque.
- `resources/icons/mirante.svg` is the source. It is the window and tray icon.
- `resources/app/mirante.ico` holds the same drawing at 16, 20, 24, 32, 40, 48, 64 and 256 pixels, for `mirante.exe`.
- `resources/app/mirante.icns` holds it at 16 to 1024 pixels, for the macOS bundle. It follows the macOS icon grid: the rounded square fills 824 of 1024 pixels, with a margin on each side, so it matches the size of other Dock icons.
- `resources/icons/mirante-tray.svg` is the monochrome menu bar icon for macOS: the same drawing without the background, with a gap between the sun and the horizon so they stay apart at 18 pixels.

## Alternatives

Five concepts were drawn and compared at app, window and tray sizes.

- **Belvedere**: a lookout pavilion on a hill. The most literal, but the pavilion is lost at 16 pixels.
- **Arcada**: one landscape seen through three arched windows, like a colonial mirante. Strong at all sizes; not chosen.
- **Monte**: an M drawn as two peaks under the sun. The clearest at tray size, but closer to a generic letter mark.
- **Luneta**: the coin viewer found at lookout points. Too detailed for small sizes.

## Consequences

- The sun and the horizon sit in the upper half, so they stay visible under the tray badge.
- The `.ico` and `.icns` files are rendered from the SVG. A change to the drawing needs new copies of both, and of the menu bar icon.
