# M9 checks by hand

These are the checks for [milestone M9](../roadmap.md#m9-built-in-panels) that tests cannot make. Run them on Windows with the release build:

```powershell
task build PRESET=release
task run PRESET=release -- -Data <a test workspace> -Settings <a scratch .ini file>
```

You need a GitLab token in a profile, a task with a CI Pipeline or MR item and a Jira item, and a second task in the same group.

Tick each box, or note what is wrong next to it.

**Result, 2026-10-30:** the user ran the checks with the release build and accepted M9. Every box is ticked.

## Adding a panel

- [x] Add Item… offers "Built-in panel" in the type grid, with the panel icon.
- [x] Its dialog has one field, Panel, with Task summary and Group overview. It has no profile, and no keys for the page.
- [x] Changing the panel in the item dialog shows the other panel.
- [x] Help > Modules lists "Built in" first, with "Built-in panel" under it.

## Task summary

- [x] It shows the task's title, its group and project, and one row per item that reports a status, in the task's order. Notes, folders and web pages have no row.
- [x] Each row has the type's short name, the item's name, the age of its status, and its summary.
- [x] An item that needs attention, such as a failed pipeline, has a red left edge and its reasons under it.
- [x] Without a token, the row shows "Token not set on this device" in grey italics.
- [x] A new status shows within a few seconds of the cell header showing it.
- [x] A click on a row, or Enter on a row reached with Tab, opens the item alone in the preview.
- [x] A task without items that report a status shows "No items in this task report a status."

## Group overview

- [x] It shows the group's name, and its active tasks by priority, each with its priority's colour, its project, and "This task" on the panel's own task.
- [x] Raising a task's priority moves it up. Marking a task Done or moving it to the trash takes it out.
- [x] A task with items that need attention has a red count, with "2 items need attention" as its tooltip.
- [x] A click on a row opens the task in the preview.

## Look and behaviour

- [x] Both panels fill their cell with the app's background, without a grey band below the rows, also in a cell 300 pixels wide.
- [x] Turning Windows to dark mode while the app runs changes the panels' colours.
- [x] With the language set to Português (Brasil), the panels' texts and ages are in Portuguese.
- [x] A panel unloaded by the memory policy shows Reload, and loads again.
- [x] A panel has no context menu, and does not follow a link outside the app.
