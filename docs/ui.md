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
- Each project's Default task is always first.
- A coloured marker shows the priority level. Low items are dimmed.
- Tasks can be expanded to show their items, so that items can be dragged onto dashboards.
- A filter hides Low items, and another hides Done and Archived tasks. The first is off by default, the second on.
- [Attention badges](#attention) appear on tasks, and are summed on projects and groups.
- **Drag and drop:**
  - Dragging within a priority level changes the order.
  - Dragging across the boundary between two levels changes the priority.
  - Dragging a task onto the content area places it in a root dashboard. See [placing tasks](#placing-tasks-and-items).
- The context menu holds new, rename, set priority, set state, delete, and "Open in new root dashboard".

## Opening tasks

- **Single click:** selects the task. It opens nothing.
- **Double click** (or `Enter`): opens the task's default dashboard in the **preview tab**. The preview tab shows its title in italics. The next task opened this way replaces it. A double click on an item opens its task the same way and focuses the item's cell.
- **Pin:** editing the dashboard, or the pin button, turns the preview tab into a normal tab. A pinned task tab is a root dashboard holding one cell with that task. It goes to the front of the tab bar. It shows two lock buttons: the root dashboard's next to the `+` button in the tab bar row, and the task dashboard's in the task header.
- Opening a task that already has a pinned tab switches to that tab instead of opening a preview.
- **Dashboard picker:** a task with several dashboards shows a small picker in its header to switch between them.

The open tabs, and which one is the preview, are stored on each device, not synced.

## Root dashboards

A root dashboard is a tab in the main tab bar. It has a name and a [layout tree](data-model.md#layout-tree).

- A cell can hold a task, showing one of the task's dashboards, or a single item from any task.
- A task inside a root dashboard shows a thin header with the task name, its dashboard picker, and a button to open the task alone.
- The `+` button creates an empty root dashboard, with editing on.
- Closing a root dashboard's tab does not delete it. It can be opened again with the [quick switcher](#keyboard).

### Placing tasks and items

- Unlock the dashboard, then drag a task or an item from the sidebar onto it. The drop overlay shows where it lands. See [editing](#editing-layouts). A locked dashboard refuses every drop: it shows no drop zones, and the cursor shows that the drop is not possible.
- Or use "Add to dashboard…" in the context menu, then pick a dashboard and click a cell. An empty cell is filled; any other cell is split.
- Tasks go only into root dashboards. Inside a task shown in a root dashboard, only that task's items can be dropped; a task dragged over its cells is refused. To place a task beside a task cell, drop it on the task cell's header, which belongs to the root dashboard.

## Task dashboards

A task dashboard has the same layout engine as a root dashboard. Its cells hold items of the task.

Each cell has a small header containing:

- the item name and an icon for its type;
- status badges;
- buttons: refresh, open in browser, detach, and a menu.

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

Layouts are locked by default, to prevent accidental changes while working. A lock button on each dashboard turns editing on. Resizing splits works even when locked.

When editing is on:

- **The cell context menu** offers:
  - split right;
  - split down;
  - wrap in tabs;
  - add tab;
  - change item;
  - clear cell;
  - remove cell.
- **The tab context menu** offers rename, close, move to the left or right, and "Collapse when single".
- **Drag and drop:** drag a cell by its header, or drag an item from the sidebar. A semi-transparent overlay shows five drop zones on the cell under the cursor:
  - the left, right, top or bottom edge splits the cell on that side;
  - the centre adds the dropped content as a tab, turning the cell into a tab group if needed. On an empty cell, the centre fills the cell instead.
- Dropping on a tab bar inserts a new tab at that position.

After each edit, the tree is [normalised](data-model.md#normalisation).

## Detached windows

Any cell can be detached into its own operating system window:

- The cell's place in the layout shows a placeholder: "Detached. Click to bring back."
- Closing the detached window puts the panel back.
- The window's position and size are stored on each device.

If the same item is shown in two places, each place has its own live view. See [decision 0003](decisions/0003-view-per-placement.md).

## Focus view

Focus answers "what should I work on now?", across all projects.

- It lists active tasks from all projects, or from one group, chosen with a filter. Default tasks are not listed.
- **Sort order:**
  1. task priority;
  2. project priority, to break ties;
  3. manual order.
- A toggle, "Needs attention first", moves tasks with attention badges to the top.
- A double click opens the task in the preview tab, as in the sidebar.

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
