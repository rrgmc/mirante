# 0052. The preview is never pinned

- Status: Accepted
- Date: 2026-10-29

## Context

Since [decision 0022](0022-task-dashboard-tabs.md), the preview could be pinned. The pin button, or an edit of the previewed dashboard's layout, moved it to a task tab of its own. [Decision 0023](0023-item-tabs.md) did the same for items, and [decision 0048](0048-focus-and-preview-slots.md) kept the pin button in the preview slot.

The user found that pinned tabs mixed task dashboards and items into the root dashboard tabs. An edit that pinned the preview by itself was a surprise. Root dashboards already show task dashboards and items next to each other, so pinned tabs added a second way to do the same thing.

## Decision

- There is one preview, in the preview slot. It never becomes a tab.
- The preview slot has no pin button, and its context menu has no "Pin" entry. It keeps its close button and "Close preview".
- Editing the previewed task dashboard's layout keeps it in the preview slot.
- The tabs on the left are root dashboards only.
- To keep a task dashboard or an item in view, the user places it on a root dashboard, for example with "Open in new root dashboard".
- No saved state brings back a pinned tab. The open tabs saved on each device have held root dashboards only since decision 0048, so nothing needs to be migrated.

## Alternatives

- **Keep the pin button but stop pinning on edits.** It removes the surprise, but keeps two kinds of tab in the root tab bar.
- **Pin into a new root dashboard.** It makes root dashboards as a side effect, which decision 0022 removed on purpose.

## Consequences

- This supersedes the pinning parts of decisions 0022, 0023 and 0048. The rest of those decisions stands: task dashboards and items open alone in the preview slot, with their title bars and, for task dashboards, their own lock.
- `MainWindow` keeps only root dashboard pages in its tab list. `MainWindow::pinTask()`, the pin button and the pin icon are gone.
- Working on two task dashboards side by side needs a root dashboard. "Open in new root dashboard" makes one in a single step.
