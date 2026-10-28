# 0011. Root dashboard cells refer to task dashboards

- Status: Accepted
- Date: 2026-10-28

## Context

A root dashboard cell can show a task. The task's layout can be changed from inside the root dashboard. It must be clear what that change affects.

## Decision

A root cell refers to one of the task's dashboards. Editing it there edits that task dashboard everywhere it is shown. To show a task differently in one root dashboard, the user creates another task dashboard and chooses it in that cell. A root cell can also hold a single item directly.

## Alternatives

- **Each root cell keeps a private copy of the task layout.** It is flexible, but the same task would drift into many different layouts, and edits would seem to disappear.

## Consequences

- One source of truth for each task dashboard.
- Tasks need a dashboard picker, and root cells store which dashboard they show.
