# 0019. Task cells show one dashboard, and items open alone

- Status: Accepted
- Date: 2026-10-28

## Context

[Decision 0011](0011-root-cells-reference-task-dashboards.md) made root dashboard cells refer to task dashboards, and said that tasks need a dashboard picker, so that a cell can switch between a task's dashboards.

Since then the sidebar lists each task's dashboards under the task, and a dashboard is what the user drags onto a root dashboard. A cell therefore already shows the dashboard the user chose when placing it. In the M2 app, the picker in the cell header made it unclear what the cell was: a task, or one of its dashboards.

Double-clicking an item in the sidebar opened its task's dashboard and focused the item there. The user wants to see the item on its own instead.

## Decision

- A task cell in a root dashboard shows the task dashboard it refers to, and has no picker. To show another dashboard of the task, drop that dashboard from the sidebar. The cell header and the tab title name both: "Task / Dashboard".
- This replaces only the picker point of decision 0011. Root cells still refer to task dashboards and do not copy them.
- A double click on a task in the sidebar only expands or collapses it. Tasks are opened through their dashboards.
- A double click on an item opens a top-level window that holds only that item, shown maximized. Escape closes it. It is not a detached cell: no dashboard changes, and it closes when the item is removed.

## Alternatives

- **Keep the picker, and also allow dragging dashboards.** Two ways to choose a cell's dashboard, one of them hidden in a small control in each cell header.
- **Open an item by maximising its cell in its task's dashboard.** It still opens the task, which is what the user wanted to avoid.

## Consequences

- A cell's dashboard changes only by replacing the cell, which is one more step for a user who switches often.
- Item windows exist from M2, before the detached windows of M4. M4 can build detached cells on the same window, with geometry stored on the device.
