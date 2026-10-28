# 0009. Four priority levels; attention as badges

- Status: Accepted
- Date: 2026-10-28

## Context

Projects and tasks differ in importance, and important ones should be listed first. Separately, live events such as a failed pipeline need action now. If live events reorder lists, the lists move while the user reads them.

## Decision

- Projects and tasks have one of four levels: Urgent, High, Normal (the default) and Low. Groups have no priority.
- Lists sort by level, then by manual order. Dragging across a level boundary changes the level.
- The Focus view sorts by task priority, then project priority, then manual order.
- Attention, meaning live events, is shown as badges. It changes order only in the Focus view, and only when the user turns that on.
- Jira priority is ignored.

## Alternatives

- **Five levels, adding "Someday",** or numbered levels P1 to P5.
- **Manual order only.**
- **Ranking by project first,** or by the higher of the project and task priorities.
- **Always sorting items that need attention first.**
- **Suggesting or syncing priority from Jira.** Jira priority is the team's view, not the user's.

## Consequences

- Lists are stable and predictable.
- An urgent task in a low-priority project still appears near the top of Focus.
