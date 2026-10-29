# 0047. Item cell headers take their group's colour

- Status: Accepted
- Date: 2026-10-29

## Context

[Decision 0046](0046-group-colours.md) tints title bars and task cell headers with the task's group colour, but keeps item cell headers in the highlight colour. On a root dashboard, an item cell placed directly in a cell then shows no group at all. The user wants to see at a glance which group such an item belongs to.

## Decision

Item cell headers take a 12% tint of the group colour of the item's task, in every dashboard: task dashboards, task cells and root dashboards. The tint follows changes to the task, its project and its group, as the task headers do. A cell with no live item, such as an empty or removed one, keeps the highlight tint.

This supersedes the part of decision 0046 that keeps item cell headers in the highlight colour. Its other parts stand.

## Alternatives

- **Only on root dashboards.** Inside a task dashboard every item belongs to the title bar's group, so a group tint there adds nothing new. But headers that change colour depending on where the cell sits would be harder to read than one rule.
- **A coloured stripe or dot instead of a tint.** It would leave the type pills' module colours alone, but the user chose tints for title bars, and one way of showing a group is easier to learn.

## Consequences

- Every item cell shows its group, also on root dashboards that mix groups.
- Item headers are lighter than task headers, 12% against 22%, so a task cell still stands out from the item cells inside it.
- A root dashboard with items from several groups shows several header colours, which is the point.
