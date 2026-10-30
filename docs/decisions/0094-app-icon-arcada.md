# 0094. The app icon is a landscape seen through three arches

- Status: Accepted
- Date: 2026-10-30

## Context

[Decision 0092](0092-app-icon-vista.md) made the app icon "Vista": a sunrise over four tiles in perspective. At small sizes, such as the taskbar, the tray and the menu bar, its thin horizon and slanted tiles did not read well.

## Decision

The icon is "Arcada", the second concept compared for 0092. It shows one landscape, with a sun and two hills, seen through three arched windows above a white sill, like the arcade of a Brazilian colonial mirante. Each window is a pane of the same view, as the cells of a dashboard are parts of one overview.

- Rounded square in blue `#1d4ed8`. The windows show a pale sky `#e0ecff`, an amber sun `#fbbf24` in the middle window, and hills in `#60a5fa` and `#1e40af`. The sill is white.
- The files are the same as in 0092: `resources/icons/mirante.svg` is the source, `resources/app/mirante.ico` is for `mirante.exe`, and `resources/app/mirante.icns` is for the macOS bundle, drawn on the macOS icon grid.
- The macOS menu bar icon, `resources/icons/mirante-tray.svg`, shows the three arches in full colour with the sun cut out of the middle one, the land below them in a lighter shade, and the sill apart below.

## Alternatives

The other concepts of 0092 still apply. Vista was tried and replaced for the reason above. **Monte**, an M drawn as two peaks, also reads at small sizes, but is closer to a generic letter mark.

## Consequences

- The three arches stay readable from 16 pixels up, because they are large, simple shapes.
- The left arch and part of the middle one stay visible under the tray badge.
- A change to the drawing still needs new copies of the `.ico`, the `.icns` and the menu bar icon.
