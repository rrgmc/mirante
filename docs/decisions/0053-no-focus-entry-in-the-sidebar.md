# 0053. The sidebar has no Focus entry

- Status: Accepted
- Date: 2026-10-29

## Context

[Decision 0048](0048-focus-and-preview-slots.md) gave Focus a fixed slot in the tab row, which is always there. It kept the Focus entry above the sidebar tree, which selected the same slot. Two controls did the same thing, and the entry took a row of the sidebar from the tree.

## Decision

- The sidebar has no Focus entry. Its header with the workspace's name is followed directly by the tree.
- Focus is opened from its slot in the tab row, and with `Ctrl+PgDown` and `Ctrl+PgUp`, which go through the tabs, then Focus, then the preview.
- Closing the last tab, or the preview while there is no tab, still shows Focus.

## Alternatives

- **Keep the entry.** It saves a move of the mouse across the window, but repeats the slot.
- **Make the entry a sidebar tree row.** It would mix a view of all tasks into the tree of groups, projects and tasks.

## Consequences

- This supersedes the point of decision 0048 that the sidebar's Focus entry selects the Focus slot. The rest of that decision stands.
- `Sidebar` loses its `focusRequested()` signal. `MainWindow::openFocus()` stays, for the slot and for tests.
