# M9 checks by hand

These are the checks for [milestone M9](../roadmap.md#m9-built-in-panels) that tests cannot make. Run them on Windows with the release build:

```powershell
task build PRESET=release
task run PRESET=release -- -Data <a test workspace> -Settings <a scratch .ini file>
```

You need a GitLab token in a profile, a task with a CI Pipeline or MR item and a Jira item, and a second task in the same group.

Tick each box, or note what is wrong next to it.

## Adding a panel

- [ ] Add Item… offers "Built-in panel" in the type grid, with the panel icon.
- [ ] Its dialog has one field, Panel, with Task summary and Group overview. It has no profile, and no keys for the page.
- [ ] Changing the panel in the item dialog shows the other panel.
- [ ] Help > Modules lists "Built in" first, with "Built-in panel" under it.

## Task summary

- [ ] It shows the task's title, its group and project, and one row per item that reports a status, in the task's order. Notes, folders and web pages have no row.
- [ ] Each row has the type's short name, the item's name, the age of its status, and its summary.
- [ ] An item that needs attention, such as a failed pipeline, has a red left edge and its reasons under it.
- [ ] Without a token, the row shows "Token not set on this device" in grey italics.
- [ ] A new status shows within a few seconds of the cell header showing it.
- [ ] A click on a row, or Enter on a row reached with Tab, opens the item alone in the preview.
- [ ] A task without items that report a status shows "No items in this task report a status."

## Group overview

- [ ] It shows the group's name, and its active tasks by priority, each with its priority's colour, its project, and "This task" on the panel's own task.
- [ ] Raising a task's priority moves it up. Marking a task Done or moving it to the trash takes it out.
- [ ] A task with items that need attention has a red count, with "2 items need attention" as its tooltip.
- [ ] A click on a row opens the task in the preview.

## Look and behaviour

- [ ] Both panels fill their cell with the app's background, without a grey band below the rows, also in a cell 300 pixels wide.
- [ ] Turning Windows to dark mode while the app runs changes the panels' colours.
- [ ] With the language set to Português (Brasil), the panels' texts and ages are in Portuguese.
- [ ] A panel unloaded by the memory policy shows Reload, and loads again.
- [ ] A panel has no context menu, and does not follow a link outside the app.
