# M1 code plan: HTML prototype

This plan implements [milestone M1](../roadmap.md#m1-html-prototype). The prototype is thrown away after M1, so the code aims to be quick to change, not to last. See [decision 0012](../decisions/0012-html-prototype-first.md).

## Shape

- Location: `prototype/` at the repository root.
- Files:
  - `index.html`: the page;
  - `style.css`: all styles;
  - `mock-data.js`: the mock groups, projects, tasks, items and dashboards;
  - `app.js`: state, rendering and behaviour.
- Plain HTML, CSS and JavaScript. No build step, no libraries, no modules. Opening `index.html` from disk works.
- The whole page is rendered again from the state after each change. The data is small, so this is fast enough and keeps the code simple.
- The state is saved to `localStorage`, so a reload keeps the user's changes. A "Reset" button restores the mock data.

## State

The mock data follows the [data model](../data-model.md): groups, projects, tasks with items and task dashboards, and root dashboards. Layout trees use the documented JSON shape. Each layout node also gets a transient `id`, used to find nodes during edits and drag and drop.

The device state ([not synced](../data-model.md#what-is-synced)) is kept apart:

| Field | Meaning |
|---|---|
| `openTabs` | Ids of the open root dashboards, in tab order |
| `active` | The active tab: `preview`, a root dashboard id, or `focus` |
| `preview` | The previewed task and its chosen dashboard |
| `unlocked` | Ids of dashboards with editing turned on |
| `detached` | Placements shown in simulated detached windows |
| `focusedCell`, `maximised` | Keyboard focus and the maximised cell |
| `expanded`, `filters` | Sidebar tree state and filters |

## Parts

1. **Sidebar.** Focus entry, then the tree of groups, projects and tasks. Tasks expand to show their items, so items can be dragged out. Priority markers, dimmed Low entries, summed attention badges, the two filters and a collapse button. Drag and drop reorders tasks and projects. Dropping on an entry of another level takes that level. A context menu holds new, rename, set priority, set state, delete, "Open in new root dashboard" and "Add to dashboard…".
2. **Tab bar.** The preview tab in italics, with a pin button, then the open root dashboards, then `+`. Double click in the sidebar pins. Turning on editing in the preview also pins it. A tab context menu offers rename and close.
3. **Layout engine.** Renders split, tabs and cell nodes recursively with flexbox. Splitters can be dragged at any time and change the `sizes` ratios. Tab groups have their own bars.
4. **Cells.**
   - An item cell has a header with type icon, name, badges, and the refresh, open, detach and menu buttons. The body is a mock summary for native types and a grey placeholder with the URL for web types.
   - A task cell in a root dashboard has a thin header with the task name, a dashboard picker, a lock button and an "open alone" button. Its body renders the task dashboard, which can be edited in place.
   - A reference to a missing entity shows "Removed".
5. **Editing.** The lock button per dashboard. The cell menu: split right, split down, wrap in tabs, add tab, change item, clear cell, remove cell. The tab menu: rename, close, move left, move right, collapse when single. Presets for new dashboards and "Apply preset". Normalisation runs after each edit, following the [rules](../data-model.md#normalisation).
6. **Drag and drop.** Sources: cell headers in edit mode, sidebar tasks and items. Over a cell, an overlay shows the five drop zones. Over a tab bar, a marker shows the insert position. A task dashboard accepts only items of its own task. Moving a cell removes it from its old place.
7. **Detach.** A detached cell leaves a placeholder in the layout, "Detached. Click to bring back." The panel appears in a floating, movable mock window. Closing the window, or clicking the placeholder, brings it back.
8. **Focus view.** Active tasks from all groups or one group, sorted by task priority, project priority and manual order. The "Needs attention first" toggle. A click opens the preview.
9. **Quick switcher.** `Ctrl+K` opens a fuzzy search over groups, projects, tasks, items and dashboards. Arrow keys and `Enter` choose.
10. **Keyboard.** The shortcuts in [ui](../ui.md#keyboard) that make sense in a browser: `Ctrl+Alt+Arrow`, `Ctrl+Tab`, `Ctrl+PgDown`, `Ctrl+PgUp`, `Ctrl+Shift+M` and `Ctrl+W`. Browsers reserve some of these, so `Alt` variants are offered as well and listed in a help overlay.

## Mock data

- Two groups: Work, with three projects, and Personal, with two.
- Every project has a Default task. Tasks cover all four priorities and all three states.
- Items cover every item type in [item types](../item-types.md). Some carry attention: a failed pipeline, a review request, an approval.
- Task dashboards include nested splits and tabs. One task has two dashboards, to show the picker.
- Two root dashboards, "Morning" and "Release", mixing task cells and single item cells.

## Checking

There are no automated tests; the prototype is thrown away. Each part is checked by hand in a browser on Windows. A headless browser run checks that the page loads without script errors.

## Out of scope

Quick add, real web pages, API data, persistence beyond `localStorage`, and real operating system windows.
