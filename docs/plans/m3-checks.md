# M3 checks by hand

These are the checks for [milestone M3](../roadmap.md#m3-data-folder-and-editing) that tests cannot make. Run them on Windows:

```powershell
tools/build.ps1
$env:PATH = "$env:MIRANTE_QT_ROOT\6.8.3\msvc2022_64\bin;$env:PATH"
build/dev/mirante.exe
```

From M3 the app writes to the workspace. Use a new workspace for these checks (File > New Workspace…), or a copy of `resources/fixtures` opened with `--data <that folder>`. Never point it at data you want to keep until these checks pass.

Tick each box, or note what is wrong next to it.

## Saving

- [ ] Rename a group, then quit and start again: the new name is there. The group's file in `groups/` is pretty-printed JSON with sorted keys and `schemaVersion`.
- [ ] Resizing a splitter or changing a layout writes the dashboard's file about half a second later, not on every mouse move.
- [ ] Deleting a task deletes its file in `tasks/`. A root dashboard that showed it shows "Removed" in that cell.
- [ ] With the sample data open (File > Open Sample Data), the warning bar says changes are not saved. Edits work, and are gone after a restart.

## Creating, editing and deleting

- [ ] File > New Group… and the `+` button in the sidebar header create a group, a project and a task.
- [ ] A new task has one dashboard, "Main", with one empty cell.
- [ ] The context menu of each kind of row (group, project, task, Items, Dashboards, item, dashboard) has the commands of [ui](../ui.md#sidebar-tree): new, edit, priority, state, delete, set as default, open in new root dashboard.
- [ ] The project dialog edits name, group, priority and folders. Exactly one folder is main. A folder without a local path shows "Folder not set on this device"; "Choose…" sets it, and it is still there after a restart. The path is not in the project's file.
- [ ] The task dialog moves a task to another project, and changes its priority and state.
- [ ] The item dialog shows the fields of the chosen type. A web page refuses a URL without `http` or `https`. A GitLab MR refuses an empty number. The `timetrack.entry` item of the fixtures shows its name only, and keeps its configuration after OK.
- [ ] New dashboard… offers the seven presets, and each builds the layout it names, filled with the task's items.
- [ ] Every delete asks first, and the question names what goes with it, such as "Delete project backend and its 4 tasks?". The last dashboard of a task cannot be deleted.

## Priorities and order

- [ ] Dragging a task within its priority level changes its place. Dragging it across the boundary into another level changes its priority and marker.
- [ ] The same works for projects. Groups can be reordered.
- [ ] A task cannot be dragged onto another project, and a task row cannot be dropped onto a dashboard.
- [ ] Set priority and Set state in the context menu work. Done tasks move to the "Done" sidebar tab.

## Focus

- [ ] The Focus entry above the tree opens a "Focus" tab at the start of the tab bar. Clicking it again switches to that tab.
- [ ] It lists active tasks, by task priority, then project priority, then manual order. The group filter works.
- [ ] A double click opens the task's default dashboard in the preview tab.
- [ ] An edit elsewhere, such as a new task, appears in the list at once.

## Images

- [ ] Copy an image, focus an empty cell of an unlocked task dashboard and press `Ctrl+V`: an Image item named "Image" fills the cell, and a PNG appears in `files/`.
- [ ] `Ctrl+V` on a focused filled cell splits it with the image on the right. On a locked dashboard or a root dashboard, the status bar explains why nothing happened.
- [ ] Pasting into a focused Image item replaces its image; the old file in `files/` is deleted.
- [ ] Deleting the Image item deletes its file.
- [ ] `Ctrl+V` in a text field of a dialog still pastes text.

## Two instances and conflicts

- [ ] Start two instances on the same workspace, each with `--data <folder>`. A rename in one appears in the other within a second.
- [ ] Deleting a task in one removes it from the other.
- [ ] Copy a task file to `tasks/<id>.sync-conflict-test.json` while the app runs. The conflict bar appears: "Two versions of task …".
- [ ] "Compare" shows both files side by side with the differing lines marked. "Keep current" deletes the copy and the bar goes.
- [ ] Make another copy with a changed title. "Use other", after confirming, replaces the task with the copy's title.

## Tabs and window

- [ ] Close and reopen the app: the same root tabs, the Focus tab if it was open, and the current tab come back, for each workspace separately.
- [ ] Window size, maximized state, sidebar width and recent workspaces are kept as in M2.
