# M2 checks by hand

These are the checks for [milestone M2](../roadmap.md#m2-app-skeleton-and-layouts) that tests cannot make. Run them on Windows with the fixtures:

```powershell
tools/build.ps1
$env:PATH = "$env:MIRANTE_QT_ROOT\6.8.3\msvc2022_64\bin;$env:PATH"
build/dev/mirante.exe
```

To check with your own copy of the data, copy `resources/fixtures` somewhere and start the app with `--data <that folder>`. M2 writes nothing, so all changes are lost when the app closes.

Tick each box, or note what is wrong next to it.

## Sidebar

- [ ] Groups, projects, tasks and items appear in that order. Projects and tasks are sorted by priority, and the Default task comes first.
- [ ] Urgent and High entries have a visible marker. Low entries are dimmed.
- [ ] "Hide done" is on at start and hides done and archived tasks. "Hide Low" hides Low projects and tasks, but not Default tasks.
- [ ] The « button hides the sidebar, and the toolbar button brings it back.

## Root tabs, preview and pinning

- [ ] At start, the tabs "Morning" and "Release" are open.
- [ ] A single click on a task only selects it. A double click opens it in the preview tab, with an italic title and a pin button. A double click on another task replaces it.
- [ ] The pin button, or an edit of the previewed dashboard, pins it: the tab moves to the front and is no longer italic.
- [ ] A double click on a task that already has a pinned tab switches to that tab.
- [ ] The `+` button opens a new, empty, unlocked root dashboard.
- [ ] Closing a root tab hides it, and `Ctrl+K` can open it again.
- [ ] "Open in new root dashboard" in a task's context menu opens a new tab with that task.

## Cells and task cells

- [ ] Each cell has a tinted header with an icon and the item name, and a body that names the item type and item. Task cell headers have a stronger tint.
- [ ] The `timetrack.entry` item (task "Add rate limiting") says it needs the `timetrack` module.
- [ ] The empty cell in the archived task "Old Terraform cleanup" says "Empty".
- [ ] In "Morning" and "Release", task cells show the task's dashboard, with a header that has the task title, a lock button and "Open alone".
- [ ] "Fix login timeout" has two dashboards. Its task cell has a picker, and choosing the other dashboard switches the cell.

## Editing layouts

Unlock a dashboard with the lock button first.

- [ ] Unlocked, every cell of the dashboard has a dashed border in the highlight colour. In a root dashboard, each task cell has one around the whole cell.

- [ ] Every cell menu command works: split right, split down, wrap in tabs, add tab, change item, clear cell, remove cell.
- [ ] Every tab menu command works: rename, close, move left, move right, collapse when single.
- [ ] Dragging a cell header onto the edge of another cell splits there, onto its centre adds a tab, and onto a tab bar inserts a tab. The drop zones are highlighted while dragging.
- [ ] Dragging an item from the sidebar works on an unlocked dashboard. A locked dashboard refuses it, with no highlight. A task dashboard refuses items of other tasks.
- [ ] Dragging a task from the sidebar over the cells inside a task cell shows no highlight and the "not allowed" cursor. Over the task cell's header it shows the zones of the whole task cell, and dropping places the task there.
- [ ] After any edit, no split has a single child, and same-direction splits are merged.
- [ ] Resizing a splitter and switching tabs work while locked, and the sizes are kept after switching root tabs.
- [ ] Editing a task dashboard inside a root dashboard changes it everywhere it is shown, including its preview.

## Keyboard

- [ ] `Ctrl+K` opens the switcher. Typing filters, and `Enter` opens the chosen task, item, root dashboard, group or project.
- [ ] `Ctrl+Alt+Arrow` moves the focus to the next cell in that direction. The focused cell has a highlighted border.
- [ ] `Ctrl+Tab` and `Ctrl+Shift+Tab` change the tab in the focused cell's tab group.
- [ ] `Ctrl+PgDown` and `Ctrl+PgUp` change the root tab.
- [ ] `Ctrl+Shift+M` maximises the focused cell, and pressing it again puts it back.
- [ ] `Ctrl+W` closes the preview tab or the current root tab.

## Load problems

- [ ] Starting with `--data` on a copy of the fixtures in which one file is broken, for example with a missing `id`, shows that file in the warning bar, and the rest loads.
