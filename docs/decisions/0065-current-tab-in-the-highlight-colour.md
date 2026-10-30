# 0065. The current tab is filled with the highlight colour

- Status: Accepted
- Date: 2026-10-30

## Context

[Decision 0041](0041-own-look-for-tabs-and-tool-buttons.md) gave tabs a quiet look: the current tab had a light grey fill and kept the normal text colour. The fill was hard to see, most of all for the Focus tab and the preview on the right of the tab row. The user asked for a clearer current tab in every tab bar, in the blue of the sidebar's selected row.

## Decision

`AppStyle` fills the current tab of every tab bar with the palette's highlight colour, as the sidebar fills its selected row. The tab's text, icon and close button are drawn in the highlighted text colour. The app's tab icons are single-colour line drawings, so the style redraws them in that colour on the current tab. The tab under the mouse keeps its light grey fill. A bar whose selection is hidden, because a tab of the other bar is current, still shows no current tab.

## Alternatives

- **A stronger grey.** It stays quiet, but it does not stand out much more than the tab under the mouse.
- **A light tint of the highlight colour with the normal text.** It is softer, but it does not match the sidebar's selection, and the user asked for the same blue.
- **Only the slot tabs.** The user asked for the same look in all tab bars.

## Consequences

- The current tab is easy to find in the root tab row, the slots, tab groups and the sidebar's foot.
- The colour follows the platform's accent colour where the style uses it, as the sidebar does.
- A tab icon in several colours would lose its colours on the current tab. All tab icons are single-colour today.
