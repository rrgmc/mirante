# 0083. Quick add can add items to an existing task, and says which it does

- Status: Accepted
- Date: 2026-10-30

## Context

[Decision 0078](0078-quick-add-dialog.md) made quick add create a new task, always, and left adding to an existing task for later. After trying it, the user wants to paste a link into a task that already exists, from that task's menu in the sidebar. The user also found that the name "Quick Add" does not say that a new task is made.

## Decision

- **Two entries, named after what they do.**
  - **New Task from Link…** (`Ctrl+Shift+N`, the File menu and the sidebar header) opens the dialog as before. Its title is "New Task from Link", and its button is **Create Task**.
  - **Add Items from Link…**, in the context menu of a task and of its Items row in the sidebar, opens the same dialog for that task. Its title is "Add Items from Link: <task title>", and its button is **Add Items**.
- **In a task,** the dialog has no Title and no Project. The project is the task's, so the finders run at once with its settings. The profile starts as the task's group's.
- **The items** are added to the task in order, as "Add to task as item" does. No dashboard changes, as for pasted images ([decision 0051](0051-paste-images-from-the-items-menu.md)). Items whose type and configuration the task already has are not offered.
- The feature keeps the name "quick add" in the documents and the code.

This supersedes the part of decision 0078 that says quick add always creates a task.

## Alternatives

- **One dialog with a choice of "new task" or "existing task".** One more decision in every use, for a choice the user already made by where they opened it.
- **Adding the items to the task's dashboard.** A dashboard's layout is the user's; new items appear in the sidebar, and the user drags them where wanted.

## Consequences

- `QuickAddRun` gains a way to add items to a task, and the dialog a mode for it.
- The names in menus, shortcuts and documents change from "Quick Add" to the two new names.
