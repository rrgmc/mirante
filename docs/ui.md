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
- **Warning bar, above the tab bar:** files that could not be loaded or saved. While the sample data is open, it also says that changes to it are not saved, and points to File > New Workspace…. See [decision 0029](decisions/0029-sample-data-read-only.md).
- **Conflict bar, under the warning bar:** one line for each conflict copy a sync tool made, such as "Two versions of task Fix login timeout", with "Keep current", "Use other" and "Compare". See [sync](sync.md#conflict-copies).

The sidebar can be collapsed.

### Sidebar tree

- Groups appear in manual order.
- Projects and tasks appear in manual order. [Priority](data-model.md#priority) does not change their place; only Focus sorts by it. See [decision 0034](decisions/0034-manual-order-in-the-sidebar.md).
- A coloured marker shows the priority level. Low items are dimmed.
- Each task has two fixed children: **Dashboards**, with its task dashboards, then **Items**, with the task's items. They are there for every task, even when empty.
- The selected row keeps its full highlight colour when the sidebar does not have the focus, so the dashboard on screen stays easy to see.
- Three tabs at the bottom choose the task list. **Projects** lists active tasks under every project, including projects without any. **Active** lists the same tasks, but leaves out projects without active tasks. **Done** lists done and archived tasks under every project. A filter button right of the workspace title opens a menu with a checkbox for each priority. Unchecking one hides the projects and tasks of that priority in all three lists. The button looks pressed while it hides something. All priorities are shown at start.
- A `+` button in the sidebar header opens a menu: New Group…, New Project… in the group of the selected row, and New Task… in the project of the selected row. The last two are disabled when the selection has no group or project.
- Two buttons in the sidebar header expand or collapse every task at once, with its Dashboards and Items. Groups and projects are left as they are; they open and close by hand only.
- [Attention badges](#attention) appear on tasks, and are summed on projects and groups.
- **Drag and drop:**
  - Dragging a group, project or task among its siblings changes its place. It never changes a priority.
  - A task itself is not dragged onto dashboards. Drag one of its items, to show that item, or one of its dashboards, to show the task with that dashboard in a root dashboard. See [placing tasks](#placing-tasks-and-items).
- The context menu holds new, edit, rename, set priority, set state and delete, and on a task dashboard "Set as default". On a task dashboard or an item it also holds "Open in new root dashboard", which makes a root dashboard showing just that dashboard or item.
- Project rows have a light tinted background, so they stand out between groups and tasks.
- Every delete asks for confirmation first.

## Window

The window opens centred on the screen, at the size it had when it was last closed on this device, maximized if it was. The sidebar width is kept too. The app's icon, a temporary one, is a blue square with three dashboard panes.

## Menus

The menu bar has three menus.

- **File:** New Workspace… (`Ctrl+N`), Open Workspace… (`Ctrl+O`), Open Recent with the last ten workspaces and Clear Recent, Rename Workspace…, Show Workspace Folder, Open Sample Data, Reload Workspace (`F5`), New Group…, and Quit (`Ctrl+Q`). A workspace is a data folder with a `workspace.mirante` file at its top. Open Workspace… chooses that file. New Workspace… chooses a folder and a name and writes the file. A folder without the file is not opened; from the menus the app offers to make it a workspace. At start the app opens the workspace given with `--data`, otherwise the last one used on this device if it still has its file, otherwise the sample data. The window title and the sidebar header show the workspace's name: the name in its `workspace.mirante`, else the folder's name. Rename Workspace…, or a double click on the name in the sidebar header, renames it, and the Store writes `workspace.mirante`. The sample data cannot be renamed. Show Workspace Folder opens the workspace's folder in Explorer, Finder or the desktop's file manager; it is disabled for the sample data, which has no folder. Reload Workspace opens the workspace again from its files. See decisions [0025](decisions/0025-workspace-file.md) and [0027](decisions/0027-workspace-needs-its-file.md).
- **View:** Sidebar (show or hide), Expand All Tasks, Collapse All Tasks, Go To… (`Ctrl+K`, the [quick switcher](#keyboard)), All Dashboards…, and New Dashboard.
- **Help:** About Mirante, with the version, the author and the project's address, and About Qt.

## Opening tasks

- **Single click:** selects a row. It opens nothing.
- **Double click** (or `Enter`) on a task: expands or collapses it. Tasks are opened through their dashboards.
- **Double click** on a task dashboard: opens it in the **preview tab**. The preview tab shows its title in italics. The next dashboard opened this way replaces it. Task dashboard titles name the task and the dashboard: "Task / Dashboard". The title bar below the tab adds the task's project, dimmed, and the tab's tooltip reads "Project / Task / Dashboard".
- **Double click** on an item: opens the item alone in a tab, the preview tab like a task dashboard, titled "Task / Item" with the item type's icon. It is read-only and can be pinned. See [decision 0023](decisions/0023-item-tabs.md).
- **Pin:** editing the dashboard's layout, or the pin button, keeps the preview open as a task tab, so the next dashboard opens in a new tab. Other changes to its task, such as a new dashboard or a new title, do not pin it. A task tab is never a root dashboard. Task tabs carry the dashboard icon, come after the root dashboard tabs, and have a title bar "Task / Dashboard" with their own lock. The lock at the end of the tab bar row is for root dashboards, and hides while a task tab is in front. See [decision 0022](decisions/0022-task-dashboard-tabs.md).
- When a task tab comes to the front, the sidebar selects its dashboard under its task.
- Opening a task dashboard that already has a tab switches to that tab instead of opening a preview.

The open tabs are stored on each device for each workspace, not synced: the root dashboard tabs and the Focus tab in order, the task dashboard in the preview tab, and the current tab. They are saved when another workspace opens and when the window closes, and come back when the workspace opens again. Pinned task tabs and item tabs are not kept. The first time a workspace opens on a device, every root dashboard opens.

## Root dashboards

A root dashboard is a tab in the main tab bar. It has a name and a [layout tree](data-model.md#layout-tree).

- A cell can hold a task, showing one of the task's dashboards, or a single item from any task.
- A task dashboard inside a root dashboard shows a thin header with the task and dashboard names, its project in dimmed text after them, such as "Rotate TLS certificates / Main infra", and a button to open it alone. It has no picker: to show another dashboard of the task, drop that dashboard from the sidebar.
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

"New dashboard" and "Apply preset" offer starting layouts. A task dashboard made with "New dashboard…" in the sidebar opens in the preview tab.

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

  Clear cell, remove cell, and deleting a tab of a tab group ask for confirmation, since they remove content from the layout.
- **The tab context menu** offers add tab (an empty tab after the clicked one), rename, "Delete tab", move to the left or right, and "Ungroup".
  - "Delete tab" removes the tab and its content from the layout.
  - "Ungroup" appears only when the group has one tab. It removes the tab group: the tab's content takes the group's place.
- **A `+` button** at the right of each tab group adds an empty tab at the end.
- **Drag and drop:** drag a cell by its header, a tab by its tab, a whole tab group by the empty part of its tab bar, or an item from the sidebar. Cell headers and tab bars show an open hand while the dashboard is unlocked. Holding a drag over a tab that is not shown opens it, so a cell can be dropped beside its content. A tab group or a split moves within its own dashboard. A semi-transparent overlay shows five drop zones on the cell under the cursor:
  - the left, right, top or bottom edge splits the cell on that side;
  - the centre adds the dropped content as a tab, turning the cell into a tab group if needed. On an empty cell, or a "Removed" one whose task or item was deleted, the centre fills the cell instead.
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

- The Focus entry in the sidebar opens Focus in a "Focus" tab at the start of the root tab bar. There is at most one Focus tab. It can be closed like any other tab.
- It lists active tasks from all projects, or from one group, chosen in a combo box. The combo box holds "All groups", then the groups in manual order.
- Each row shows the task title, then its project and group, dimmed, and the [priority](data-model.md#priority) marker of the sidebar. Low tasks are dimmed.
- A right click on a task opens the task's menu from the sidebar: edit, set priority, set state, delete and the rest.
- **Sort order:**
  1. task priority;
  2. project priority, to break ties;
  3. manual order.
- The list follows changes to groups, projects and tasks as they happen.
- A double click or `Enter` opens the task's default dashboard in the preview tab.
- A toggle, "Needs attention first", moves tasks with attention badges to the top. It arrives with attention in M5.

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
| `Ctrl+V` | With an image on the clipboard: paste it into the focused cell's image item, or as a new image item on an unlocked task dashboard. See [image](item-types.md#image). A text field keeps its own paste. |
| Global hotkey (configurable) | Bring Mirante to the front |

On macOS, `Cmd` replaces `Ctrl`.

A web page with keyboard focus receives keys first. App shortcuts are registered at window level so that they still work. Shortcuts the page needs, such as `Ctrl+K` inside a web app, can be released per item.

The global hotkey uses the QHotkey library. It works on Windows, macOS and X11, but not on Wayland, where the user binds a desktop shortcut instead.

## Tray and notifications

- Closing the main window hides it to the system tray. Polling and notifications continue. "Quit" in the tray menu exits.
- The tray icon shows the number of tasks that need attention.
- If the platform has no system tray, for example GNOME without an extension, closing the window quits the app.
- A desktop notification appears only when a pipeline linked to a task fails. Clicking it opens the task.
