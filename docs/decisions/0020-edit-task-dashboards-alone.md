# 0020. Task dashboards are edited only on their own

- Status: Accepted
- Date: 2026-10-28

## Context

[Decision 0011](0011-root-cells-reference-task-dashboards.md) made root dashboard cells refer to task dashboards, and let the user edit a task dashboard inside a root dashboard: edits in the nested view changed the task dashboard everywhere.

In the M2 app this meant two locks on one screen, one for the root dashboard and one in each task cell, and drops that could land in either the root dashboard or a task dashboard depending on where the cursor was. The user found editing task dashboards inside root dashboards confusing.

## Decision

- Inside a root dashboard, a task cell is read-only. It has no lock button, its cells have no edit menus, and nothing can be dropped into it.
- A task dashboard is edited only on its own: in the preview tab, or in a root tab that pins it, with that tab's lock. A double click on a task cell, its "Edit task dashboard" button, or "Edit task dashboard" in its menu opens it there.
- The root dashboard's own editing is unchanged: task cells are still placed, moved, split and removed in it while it is unlocked.
- This replaces the editing point of decision 0011. Root cells still refer to task dashboards and do not copy them, so an edit made on its own shows in every root dashboard that holds it.

## Alternatives

- **Keep editing in place, with a clearer lock.** The two-level lock and the two possible drop targets remain, which was the source of the confusion.
- **Open task dashboards for editing in a separate window.** It adds a window to manage for every edit; the preview tab already shows one task dashboard on its own.

## Consequences

- Rearranging a task dashboard seen in a root dashboard takes one extra step: opening it.
- Every drop and every menu in a root dashboard acts on the root dashboard, so the drop overlay and the menus have one meaning per screen.
