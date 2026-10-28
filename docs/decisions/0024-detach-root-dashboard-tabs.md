# 0024. Only root dashboard tabs can be detached

- Status: Accepted
- Date: 2026-10-28

## Context

[ui.md](../ui.md) said that any cell can be detached into its own operating system window, with a placeholder left in its place. The user decided that this is not what they want: a separate window is for a whole root dashboard, not for one cell.

## Decision

- A root dashboard tab can be detached into a new top-level window, which shows that root dashboard with its own lock. Closing the window puts the tab back in the main window.
- Nothing else is detached: not cells, not task dashboard tabs, not item tabs.
- Each detached window's position and size are stored on the device, as before.
- This arrives in M4, which already held detaching.

## Alternatives

- **Detach cells, as first planned.** It needs a placeholder in the layout and per-cell windows, for a use the user does not have.
- **Detach any tab.** Task dashboards and items open in the preview tab and are short-lived; a window per such tab adds windows without a clear need.

## Consequences

- Cell headers have no detach button.
- A placement is now a cell or an item tab; a detached root dashboard holds placements like the main window does.
