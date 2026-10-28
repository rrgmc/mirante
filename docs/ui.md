# User interface

This document describes navigation, dashboards and editing. It was checked with the clickable HTML prototype in [milestone M1](roadmap.md#m1-html-prototype).

## Main window

```
┌───────────────┬──────────────────────────────────────────────┐
│ Focus         │ [Preview: Fix login] [Morning] [Release] [+] │
│               ├──────────────────────────────────────────────┤
│ ▾ Work        │                                              │
│   ▾ backend   │        active root dashboard or task         │
│     Default   │        (splits, tabs and cells)              │
│     ● Fix ... │                                              │
│     ○ Add ... │                                              │
│   ▸ frontend  │                                              │
│ ▸ Personal    │                                              │
└───────────────┴──────────────────────────────────────────────┘
```

- **Sidebar, on the left:**
  - A **Focus** entry at the top. See [Focus view](#focus-view).
  - A tree of groups, projects and tasks.
- **Tab bar, on the right:** root dashboards and the preview tab.
- **Content area:** the dashboard of the selected tab.

The sidebar can be collapsed.

### Sidebar tree

- Groups appear in manual order.
- Projects and tasks are sorted by [priority](data-model.md#priority), then manual order.
- A coloured marker shows the priority level. Low items are dimmed.
- Each task has two fixed children: **Dashboards**, with its task dashboards, then **Items**, with the task's items. They are there for every task, even when empty.
- The selected row keeps its full highlight colour when the sidebar does not have the focus, so the dashboard on screen stays easy to see.
- Three tabs at the bottom choose the task list. **Projects** lists active tasks under every project, including projects without any. **Active** lists the same tasks, but leaves out projects without active tasks. **Done** lists done and archived tasks under every project. A filter, off by default, hides Low items in all three.
- Two buttons in the sidebar header expand or collapse every task at once, with its Dashboards and Items. Groups and projects are left as they are; they open and close by hand only.
- [Attention badges](#attention) appear on tasks, and are summed on projects and groups.
- **Drag and drop:**
  - Dragging within a priority level changes the order.
  - Dragging across the boundary between two levels changes the priority.
  - A task itself is not dragged onto dashboards. Drag one of its items, to show that item, or one of its dashboards, to show the task with that dashboard in a root dashboard. See [placing tasks](#placing-tasks-and-items).
- The context menu holds new, rename, set priority, set state, delete, and "Open in new root dashboard".

## Menus

The menu bar has three menus.

- **File:** Open Workspace… (`Ctrl+O`), Open Recent with the last ten workspaces and Clear Recent, Open Sample Data, Reload Workspace (`F5`), and Quit (`Ctrl+Q`). A workspace is a data folder. At start the app opens the workspace given with `--data`, otherwise the last one used on this device, otherwise the sample data. The window title names the workspace.
- **View:** Sidebar (show or hide), Expand All Tasks, Collapse All Tasks, Go To… (`Ctrl+K`, the [quick switcher](#keyboard)), All Dashboards…, and New Dashboard.
- **Help:** About Mirante, with the version, the author and the project's address, and About Qt.

## Opening tasks

- **Single click:** selects a row. It opens nothing.
- **Double click** (or `Enter`) on a task: expands or collapses it. Tasks are opened through their dashboards.
- **Double click** on a task dashboard: opens it in the **preview tab**. The preview tab shows its title in italics. The next dashboard opened this way replaces it. Task dashboard titles name the task and the dashboard: "Task / Dashboard".
- **Double click** on an item: opens the item alone in a tab, the preview tab like a task dashboard, titled "Task / Item" with the item type's icon. It is read-only and can be pinned. See [decision 0023](decisions/0023-item-tabs.md).
- **Pin:** editing the dashboard, or the pin button, keeps the preview open as a task tab, so the next dashboard opens in a new tab. A task tab is never a root dashboard. Task tabs carry the dashboard icon, come after the root dashboard tabs, and have a title bar "Task / Dashboard" with their own lock. The lock at the end of the tab bar row is for root dashboards, and hides while a task tab is in front. See [decision 0022](decisions/0022-task-dashboard-tabs.md).
- When a task tab comes to the front, the sidebar selects its dashboard under its task.
- Opening a task dashboard that already has a tab switches to that tab instead of opening a preview.

The open tabs, and which one is the preview, are stored on each device, not synced.

## Root dashboards

A root dashboard is a tab in the main tab bar. It has a name and a [layout tree](data-model.md#layout-tree).

- A cell can hold a task, showing one of the task's dashboards, or a single item from any task.
- A task dashboard inside a root dashboard shows a thin header with the task and dashboard names, its lock, and a button to open it alone. It has no picker: to show another dashboard of the task, drop that dashboard from the sidebar.
- The `+` button creates an empty root dashboard, with editing on.
- Closing a root dashboard's tab, with its close button, a middle click or the tab bar's context menu, does not delete it. It can be opened again with the [quick switcher](#keyboard) or the dashboard list.
- The **dashboard list**, opened with the button after the lock in the tab bar row or with "All dashboards…" in the tab bar's context menu, lists every root dashboard and marks closed ones. It opens a dashboard again, and it is the only place where a root dashboard is deleted, after a confirmation.
- Right-clicking the tab bar shows "New dashboard", "Close tab" over a tab, and "All dashboards…".

### Placing tasks and items

- Unlock the dashboard, then drag an item or a task dashboard from the sidebar onto it. A task dashboard dropped on a root dashboard places its task there, showing that dashboard. The drop overlay shows where it lands. See [editing](#editing-layouts). A locked dashboard refuses every drop: it shows no drop zones, and the cursor shows that the drop is not possible.
- Or use "Add to dashboard…" in the context menu, then pick a dashboard and click a cell. An empty cell is filled; any other cell is split.
- Tasks go only into root dashboards. A task dashboard shown in a root dashboard is read-only there: it has no lock, no edit menus, and takes no drops. A drop on it acts on the root dashboard. To change a task dashboard, open it on its own with a double click on its cell, its "Edit task dashboard" button or its menu, and edit it there. See [decision 0020](decisions/0020-edit-task-dashboards-alone.md).

## Task dashboards

A task dashboard has the same layout engine as a root dashboard. Its cells hold items of the task.

Each cell has a small header containing:

- the item name and an icon for its type;
- in a root dashboard, where an item can come from any task, its project and task in dimmed text after the name, such as "backend / Overview";
- status badges;
- buttons: refresh, open in browser, and a menu.

### Layout presets

"New dashboard" and "Apply preset" offer starting layouts:

- Full: one cell.
- Two side by side.
- Two stacked.
- Grid of four.
- One large on top, two below.
- One large on the left, two stacked on the right.
- Tabs: one tab group with one cell per item.

A preset only creates the first tree. The user can change it freely afterwards.

## Editing layouts

Layouts are locked by default, to prevent accidental changes while working. The lock button at the far right of the tab bar row turns editing on for the current tab. Task dashboards shown inside a root dashboard have no lock of their own; they are edited on their own tab. Resizing splits works even when locked.

When editing is on:

- **The cell context menu** offers:
  - split right;
  - split down;
  - wrap in tabs;
  - add tab;
  - clear cell;
  - remove cell.
- **The tab context menu** offers add tab (an empty tab after the clicked one), rename, close, move to the left or right, "Remove tab group" and "Collapse when single".
  - "Remove tab group" is enabled when the group has one tab. It unwraps the group once: the tab's content takes the group's place.
  - "Collapse when single" is a setting of the group: whenever it has one tab, now or after closing others, the group is replaced by that tab's content.
- **A `+` button** at the right of each tab group adds an empty tab at the end.
- **Drag and drop:** drag a cell by its header, or drag an item from the sidebar. A semi-transparent overlay shows five drop zones on the cell under the cursor:
  - the left, right, top or bottom edge splits the cell on that side;
  - the centre adds the dropped content as a tab, turning the cell into a tab group if needed. On an empty cell, the centre fills the cell instead.
- Dropping on a tab bar inserts a new tab at that position.
- **Replace:** holding `Ctrl` while dragging makes the drop replace the content of the cell under the cursor, instead of splitting it or adding a tab. The whole cell is highlighted. A moved cell leaves its old place.

After each edit, the tree is [normalised](data-model.md#normalisation).

## Detached windows

A root dashboard tab can be detached into its own operating system window. Nothing else is detached: not cells, not task dashboard tabs, not item tabs. See [decision 0024](decisions/0024-detach-root-dashboard-tabs.md).

- The detached window shows the root dashboard, with its own lock.
- Closing the window puts the tab back in the main window.
- The window's position and size are stored on each device.

If the same item is shown in two places, each place has its own live view. See [decision 0003](decisions/0003-view-per-placement.md).

## Focus view

Focus answers "what should I work on now?", across all projects.

- It lists active tasks from all projects, or from one group, chosen with a filter.
- **Sort order:**
  1. task priority;
  2. project priority, to break ties;
  3. manual order.
- A toggle, "Needs attention first", moves tasks with attention badges to the top.
- A double click opens the task's default dashboard in the preview tab.

## Attention

Attention means something needs action now. Sources:

- a pipeline failed;
- someone requested your review;
- your merge request was approved, or has conflicts;
- a linked Jira issue changed status.

Attention appears as badges. It changes the order only in the Focus view, and only when its toggle is on. See [decision 0009](decisions/0009-priority-and-attention.md).

## Quick add

Quick add creates a task from a link. Open it with a toolbar button or `Ctrl+Shift+N`, then paste a link. What it recognises depends on the [modules](modules.md#quick-add) in the build. With the `gitlab` and `jira` modules:

- **A Jira key or issue URL:**
  - The task is named after the issue summary.
  - A Jira Issue item is added.
  - The app searches for merge requests whose branch or title contains the key, and offers to add them.
- **A GitLab merge request URL:**
  - The task is named after the merge request title.
  - An MR summary item and a CI Pipeline item for its branch are added.
  - A Jira key found in the title or branch adds the issue too.

The project is chosen by matching the merge request's GitLab project or the issue's Jira project key against the projects' module settings. If no project matches, or several do, the app asks.

## Keyboard

| Key | Action |
|---|---|
| `Ctrl+K` | Quick switcher: fuzzy search over groups, projects, tasks, items and dashboards |
| `Ctrl+Shift+N` | Quick add |
| `Ctrl+Alt+Arrow` | Move focus to the cell in that direction |
| `Ctrl+Tab`, `Ctrl+Shift+Tab` | Next or previous tab in the focused tab group |
| `Ctrl+PgDown`, `Ctrl+PgUp` | Next or previous root dashboard tab |
| `Ctrl+Shift+M` | Maximise the focused cell, or restore it |
| `Ctrl+W` | Close the preview tab or the focused root tab |
| Global hotkey (configurable) | Bring Mirante to the front |

On macOS, `Cmd` replaces `Ctrl`.

A web page with keyboard focus receives keys first. App shortcuts are registered at window level so that they still work. Shortcuts the page needs, such as `Ctrl+K` inside a web app, can be released per item.

The global hotkey uses the QHotkey library. It works on Windows, macOS and X11, but not on Wayland, where the user binds a desktop shortcut instead.

## Tray and notifications

- Closing the main window hides it to the system tray. Polling and notifications continue. "Quit" in the tray menu exits.
- The tray icon shows the number of tasks that need attention.
- If the platform has no system tray, for example GNOME without an extension, closing the window quits the app.
- A desktop notification appears only when a pipeline linked to a task fails. Clicking it opens the task.
