# 0018. No Default task in projects

- Status: Accepted
- Date: 2026-10-28

## Context

The data model gave every project exactly one Default task. It could not be deleted, renamed or archived. It held items that concern the whole project, such as "all open merge requests of this repository", and it was always listed first.

The user tried the M2 app with this rule and asked to remove it. A task that exists only because a rule says so is noise in the sidebar, and its special treatment (always first, never hidden, no priority marker) made the sidebar harder to read.

## Decision

- Projects have no Default task. A project may have any number of tasks, including none.
- Items that concern the whole project go in an ordinary task, which the user creates and names, such as "Overview".
- `Task` has no `isDefault` field. A file written before this decision keeps its `isDefault` key as an unknown field and writes it back unchanged.
- Sorting has no special case: tasks are ordered by priority, then `order`, then title. The filters treat every task the same.

## Alternatives

- **Keep the Default task, but hide it when it is empty.** It keeps the special cases in sorting, filters and editing, and the rule still decides where the user's items go.
- **Let projects hold items directly.** Items would then live in two places, and dashboards would need a second kind of owner. A task already gives items a home with its dashboards.

## Consequences

- The sidebar, sorting and Focus have no special case for one task per project.
- M3 does not need to create a task with every project, or to prevent its deletion.
- Quick add cannot assume a task to put project-wide items in. When M7 designs it, it asks for a task or creates one.
- The fixtures keep their former Default tasks as ordinary tasks titled "Overview", because root dashboards refer to their items.
