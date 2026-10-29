# 0034. The sidebar keeps the manual order; priority is a marker

- Status: Accepted
- Date: 2026-10-29
- Supersedes: the sorting part of [0009](0009-priority-and-attention.md)

## Context

Decision 0009 sorted projects and tasks by priority, then by manual order, and a drag across a priority boundary changed the priority. In use, a drag meant to reorder changed priorities by surprise, and a new priority moved an entry away from where the user had put it. The user wants to arrange the sidebar by hand, and sort by priority only when they choose to.

## Decision

- The sidebar lists projects and tasks by the manual `order` field only. The priority shows as the coloured marker.
- A drag changes only the place of a group, project or task. It never changes a priority.
- Setting a priority changes only the marker. The entry keeps its place.
- A new project or task goes last among its siblings.
- A filter button right of the workspace title has a checkbox for each priority, and replaces the "Hide Low" button.
- The Focus view still sorts by task priority, then project priority, then manual order, as in 0009.

## Alternatives

- **Keep sorting by priority, and show a hint while dragging.** Tried first: a tooltip named the new priority. It made the change visible but kept the surprise.
- **Sort by priority as an option.** Not needed now; the user can order by priority by hand.

## Consequences

- `order` now numbers all siblings, not one priority level. Data from before keeps its numbers, so entries of different levels with the same `order` sort by name until the user moves one. The sample data was renumbered to keep its old look.
- `edits::moveProject` and `edits::moveTask` no longer take a priority.
