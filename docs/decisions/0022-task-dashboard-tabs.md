# 0022. Task dashboards open in their own kind of tab

- Status: Accepted; place of the preview superseded by 0048; pinning superseded by [0052](0052-no-pinning.md)
- Date: 2026-10-28

## Context

[ui.md](../ui.md#opening-tasks) defined a pinned task tab as a root dashboard holding one cell with the task. Pinning, by the pin button or by editing the preview, created that root dashboard.

Since [decision 0020](0020-edit-task-dashboards-alone.md), a task dashboard inside a root dashboard is read-only. Editing the preview pinned it, so the dashboard the user was editing turned into a read-only root tab. The user also found it unclear which tabs were root dashboards and which were task dashboards, and that the task dashboard used the root dashboard's lock.

## Decision

- A task dashboard opens in a task tab, never as a root dashboard. The preview is a task tab that the next opened task dashboard replaces. Pinning, by the pin button or by editing it, keeps it open instead; nothing is added to the root dashboards.
- Task tabs carry the dashboard icon and come after the root dashboard tabs. They are not in the dashboard list, which holds root dashboards only. Which task tabs are open is kept on each device, like the open root tabs.
- A task tab has a title bar, "Task / Dashboard", with its own lock. The lock at the end of the tab bar row is a root dashboard's, and it hides while a task tab is in front.
- When a task tab comes to the front, the sidebar selects that dashboard under its task.
- A root dashboard that shows a task is made explicitly, with "Open in new root dashboard" or by dropping a task dashboard on a root dashboard.

## Alternatives

- **Keep pinning as a root dashboard, and make that cell editable.** It brings back editing inside root dashboards, which decision 0020 removed.
- **No pinning: only one task tab at a time.** Working on two tasks side by side would need a root dashboard for each.

## Consequences

- The tab bar holds two kinds of tab, told apart by the icon and by the title bar of task tabs.
- Root dashboards are only ever made on purpose, so the dashboard list holds no dashboards made as a side effect of pinning.
