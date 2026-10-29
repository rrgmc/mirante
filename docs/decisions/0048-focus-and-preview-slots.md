# 0048. Focus and the preview have fixed slots right of the tabs

- Status: Accepted; the pin button and pinned tabs superseded by [0052](0052-no-pinning.md); the sidebar's Focus entry by [0053](0053-no-focus-entry-in-the-sidebar.md)
- Date: 2026-10-29

## Context

Since [decision 0022](0022-task-dashboard-tabs.md), the preview was a tab at the end of the root tab bar, and since [decision 0023](0023-item-tabs.md) items opened there too. The user found that the preview looked like one more root dashboard tab. Focus was a closable tab at the start of the same bar, so it also looked like a dashboard.

## Decision

- The tab row has two fixed **slots** on the right, before the lock: Focus and the preview. They look like tabs and sit together, after a thin separator, apart from the tabs on the left.
- The tabs on the left are root dashboards, pinned task dashboards and pinned items, as before, with the + and dashboard list buttons after them.
- Every preview opens in the preview slot and replaces the previous one: task dashboards and items from the sidebar, "Open Alone", the quick switcher, Focus and new task dashboards. The slot shows the preview's icon and its title in italics, with a pin button and a close button. Pinning moves the page to a new tab at the end of the left tabs. With no preview, the slot is hidden.
- The Focus slot is always there and cannot be closed. The sidebar's Focus entry selects it.
- One page is current across the tabs and the slots. While a slot is current, no left tab shows as selected, and the other way round.
- `Ctrl+PgDown` and `Ctrl+PgUp` go through the left tabs, then Focus, then the preview, and wrap. `Ctrl+W` closes the preview or the current tab; it does nothing on Focus.
- The open tabs saved on each device hold the root dashboard tabs only. If Focus was current, it is current again next time. The preview is not kept, as before.

## Alternatives

- **Keep the preview as the last tab, but styled differently.** It still moves along as tabs open and close, and it still reads as a tab of the same bar.
- **Show the preview slot disabled when empty.** It would keep the row steady, but an empty slot is a target that does nothing and takes room from the tabs.
- **A row of checkable buttons instead of a second tab bar.** The slots need the tab look, an italic title and tab buttons for pin and close, which a `QTabBar` already has with [AppStyle](0041-own-look-for-tabs-and-tool-buttons.md).

## Consequences

- This supersedes the place of the preview in decisions 0022 and 0023: the preview is no longer a tab of the root tab bar. The rest of those decisions stands.
- `MainWindow` keeps one current page across two bars: `RootTabBar` on the left and `SlotTabBar` on the right. Each bar can show no selection while the other holds the current page.
- Focus is no longer a tab, so it cannot be closed or reordered, and it is not in the saved tab list. A "focus" id saved by earlier versions is skipped.
