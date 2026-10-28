# M2 checks by hand

These are the checks for [milestone M2](../roadmap.md#m2-app-skeleton-and-layouts) that tests cannot make. Run them on Windows with the fixtures:

```powershell
tools/build.ps1
$env:PATH = "$env:MIRANTE_QT_ROOT\6.8.3\msvc2022_64\bin;$env:PATH"
build/dev/mirante.exe
```

To check with your own copy of the data, copy `resources/fixtures` somewhere and start the app with `--data <that folder>`. M2 writes nothing, so all changes are lost when the app closes.

Tick each box, or note what is wrong next to it.

## Menus and workspaces

- [ ] File > Open Workspace… opens a data folder; the window title names it. Open Recent lists it, most recent first, and Clear Recent empties the list.
- [ ] Restarting the app opens the last workspace. Open Sample Data goes back to the built-in data. Reload Workspace (`F5`) loads the folder again.
- [ ] A recent workspace whose folder is gone shows a warning and leaves the list.
- [ ] The sidebar header shows the workspace's name. File > Rename Workspace… or a double click on the name renames it; the name is kept in the folder's `workspace.json` and comes back after reopening. The sample data cannot be renamed.
- [ ] View has Sidebar, Expand All Tasks, Collapse All Tasks, Go To… (`Ctrl+K`), All Dashboards… and New Dashboard, and each works.
- [ ] Help > About Mirante shows Mirante 0.1, Rangel Reale with the email address, the project's address and the license.

## Window

- [ ] The window and `mirante.exe` show the Mirante icon.
- [ ] After resizing the window and the sidebar and restarting, the window opens centred at the new size, with the same sidebar width, and maximized if it was.

## Sidebar

- [ ] Project rows have a tinted background. "Open in new root dashboard" is on the context menu of task dashboards and items, not tasks.
- [ ] Groups, projects and tasks appear in that order, and each task has the children "Dashboards" and "Items", in that order. The selected row stays in full colour when the sidebar loses the focus. Projects and tasks are sorted by priority, then their manual order.
- [ ] Urgent and High entries have a visible marker. Low entries are dimmed.
- [ ] The sidebar has three tabs at the bottom: "Projects" (chosen at start: active tasks under every project), "Active" (the same without projects that have no active task) and "Done" (done and archived tasks). "Hide Low" works in all three.
- [ ] The expand and collapse buttons in the sidebar header open or close every task; collapse leaves groups and projects open.
- [ ] The « button hides the sidebar, and the » button at the start of the tab bar row brings it back.

## Root tabs, preview and pinning

- [ ] At start, the tabs "Morning" and "Release" are open.
- [ ] A single click only selects. A double click on a task only expands or collapses it.
- [ ] A double click on a task dashboard opens it in the preview tab, titled "Task / Dashboard" in italics, with a pin button. A double click on another dashboard replaces it.
- [ ] A double click on an item opens it alone in the preview tab, titled "Task / Item" with its type icon, read-only and without a lock. Opening it again switches to its tab, and its pin button keeps it.
- [ ] The pin button, or an edit of the previewed dashboard, pins it: the tab stays open, no longer italic, and the next dashboard opens in a new tab. No root dashboard is added.
- [ ] Task tabs have the dashboard icon and a title bar with their own lock; the lock at the end of the tab bar row hides while a task tab is in front.
- [ ] When a task tab comes to the front, the sidebar selects its dashboard under its task.
- [ ] A double click on a task dashboard that already has a tab switches to that tab.
- [ ] The `+` button opens a new, empty, unlocked root dashboard.
- [ ] Closing a root tab, with its close button, a middle click or the tab bar's context menu, hides it. `Ctrl+K` or the dashboard list opens it again.
- [ ] The lock sits at the far right of the tab bar row. The dashboard list button, after `+`, lists every root dashboard, marks closed ones, opens them, and deletes one only after a confirmation.
- [ ] Right-clicking the tab bar shows "New dashboard", "Close tab" over a tab, and "All dashboards…".
- [ ] "Open in new root dashboard" in a task's context menu opens a new tab with that task.

## Cells and task cells

- [ ] Each cell has a tinted header with an icon and the item name, and a body that names the item type and item. Task cell headers have a stronger tint.
- [ ] The `timetrack.entry` item (task "Add rate limiting") says it needs the `timetrack` module.
- [ ] The empty cell in the archived task "Old Terraform cleanup" says "Empty".
- [ ] In "Morning" and "Release", task cells show the task's dashboard, read-only, with a header that has the task title and an "Edit task dashboard" button, and no lock.
- [ ] A double click on a task cell, its button, or "Edit task dashboard" in its right-click menu (locked or not) opens that task dashboard on its own, where its lock allows editing.
- [ ] An item placed directly in a root dashboard shows its project and task, dimmed, after its name, such as "Review requested backend / Overview".
- [ ] Task cells have no dashboard picker. Their header reads "Task / Dashboard", such as "Fix login timeout / Work".

## Editing layouts

Unlock a dashboard with the lock button first.

- [ ] Unlocked, every cell of the dashboard has a dashed border in the highlight colour. In a root dashboard, each task cell has one around the whole cell.

- [ ] Every cell menu command works: split right, split down, wrap in tabs, add tab, clear cell, remove cell. There is no "Change item".
- [ ] Clear cell, Remove cell, and Close on a tab of a tab group ask for confirmation first.
- [ ] Holding `Ctrl` while dragging an item, a task dashboard or a cell highlights the whole cell under the cursor, and dropping replaces that cell's content. Without `Ctrl` the usual edges, centre and tab bars apply.
- [ ] Every tab menu command works: add tab, rename, close, move left, move right, remove tab group (only with one tab), collapse when single.
- [ ] Unlocked, each tab group has a `+` button on the right that adds an empty tab at the end. Locked, it is hidden.
- [ ] Dragging a cell header onto the edge of another cell splits there, onto its centre adds a tab, and onto a tab bar inserts a tab. The drop zones are highlighted while dragging.
- [ ] Dragging an item from the sidebar works on an unlocked dashboard. A locked dashboard refuses it, with no highlight. A task dashboard refuses items of other tasks.
- [ ] A task row in the sidebar cannot be dragged. Its "Dashboards" children can: dropping one on a root dashboard places the task there with that dashboard.
- [ ] Nothing can be dropped into a task cell's own dashboard. With the root dashboard unlocked, a drop over a task cell shows the zones of the whole task cell and acts on the root dashboard.
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
