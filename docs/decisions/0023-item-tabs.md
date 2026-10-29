# 0023. Items open alone in a tab

- Status: Accepted; place of the preview superseded by 0048
- Date: 2026-10-28

## Context

[Decision 0019](0019-fixed-task-cells-and-item-windows.md) made a double click on an item in the sidebar open a maximized top-level window that held only the item. The user meant something else: the item should open the way a task dashboard does, in a tab of the main window.

## Decision

- A double click on an item in the sidebar opens it alone in a tab: the preview tab, like a task dashboard, or its own tab if it already has one. It replaces the item window point of decision 0019; the rest of 0019 stands.
- An item tab shows only the item, read-only, under a title bar "Task / Item". It carries the item type's icon. It has no lock, because there is no layout to edit.
- An item tab is pinned like a task dashboard's preview: by its pin button. It closes when its item is removed.
- There is no separate item window.

## Alternatives

- **Keep the item window.** A second top-level window for every item opened, which the user did not want.
- **Focus the item's cell in its task dashboard.** It opens the whole task dashboard, which is what opening a dashboard is for.

## Consequences

- The tab bar holds three kinds of tab: root dashboards, task dashboards and items, told apart by their icons.
