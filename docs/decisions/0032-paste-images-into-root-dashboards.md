# 0032. Pasting an image into a root dashboard asks for its task

- Status: Accepted
- Date: 2026-10-29

## Context

A pasted image becomes an Image item, and every item belongs to one task. A task dashboard names its task, but a root dashboard can show items and tasks from anywhere, so it has no task of its own. In M3, `Ctrl+V` on a root dashboard therefore did nothing but show "Paste images into a task dashboard." The user wants to build root dashboards straight from screenshots.

## Decision

`Ctrl+V` with an image on an unlocked root dashboard opens a dialog that asks which task the image belongs to. It lists the active tasks as "Project / Task", in sidebar order, with the task of the sidebar's selection chosen. The new item is added to that task, and the root dashboard's cell refers to it with that task, placed as on a task dashboard: in an empty focused cell, or to the right of another focused cell.

## Alternatives

- **Keep refusing, with a clearer message.** Nothing to choose, but every screenshot needs a detour through a task dashboard.
- **Use the sidebar's selection without asking.** Faster, but an image could land in a task the user did not mean, with no sign of where it went.

## Consequences

- The item shows in the chosen task's Items, like any other item of that task.
- A paste into a root dashboard is two writes: the task's file and the root dashboard's file.
