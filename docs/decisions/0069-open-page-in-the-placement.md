# 0069. "Open page" is shown by the placement

- Status: Accepted
- Date: 2026-10-30

## Context

The "Open page" button of a native summary opens the matching web page in the same cell, and the user needs a way back. A native view could make the page itself with `ItemContext::createWebView()` inside its own widgets. But the `ViewPool` only handles the views of web types, so such a page would never be unloaded, and the view would have to manage the toolbar in the cell header.

## Decision

- `ItemContext::showPage(url)` shows a web page in place of the view, in the item's profile. The view stays alive behind it, so it keeps its scroll position and selection.
- The cell header shows the page's toolbar with a "Back to summary" button before it. The button drops the page and shows the view again.
- The page is in the `ViewPool` like any other web view. When the pool unloads it, the cell goes back to the view instead of showing the Reload placeholder.
- The page is not stored. After a restart, or when the view is made again, the cell shows the view.
- Holding `Shift` opens the address in the external browser instead. The module does this itself with `QDesktopServices`, and does not call `showPage()`.

## Alternatives

- **The view makes the page itself.** No core change, but pages are not unloaded, and each module repeats the same code.
- **Open the page type in the preview slot.** Simple, but the user loses their place, and it contradicts [item types](../item-types.md#light-actions).
- **Change the item's type to the page type.** It would write a synced file for a passing look.

## Consequences

- `Placement` keeps a second widget and a second pool entry state.
- A native type needs no web code, and can open a page in one line.
