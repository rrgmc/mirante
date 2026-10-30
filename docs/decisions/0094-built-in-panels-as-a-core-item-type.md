# 0094. Built-in panels are one core item type, and see only their own task and group

- Status: Proposed
- Date: 2026-10-30

## Context

[Web views](../web-views.md#built-in-panels) describes built-in panels: HTML pages shipped with the app, served from `dash://` in the app profile, which read the status cache through a `QWebChannel` object named `dash`. [Item types](../item-types.md#built-in-panels) names the type `dash.panel` and two panels, Task summary and Group overview. Several things are still open:

- how the core registers a type when every other type comes from a module;
- how a panel reaches the model, the status cache and the window, which `ItemContext` does not offer;
- which task and group a panel shows;
- what keeps a page that is not a panel away from the bridge.

## Decision

- **A built-in module.** The core registers its own module, `dash`, before the enabled modules, with one type, `dash.panel`. The registry marks it as built in. The Modules window lists it under "Built in", and the type picker offers it like any other type. It cannot be left out of a build.
- **One type, a choice of panel.** `dash.panel` has one field, `panel`, a choice of `task-summary` and `group-overview`, required. Each value is a folder under `dash://panels/`. A panel added later is a new value, not a new type.
- **The core makes the view.** `dash.panel` lives in the items library, which can reach the placement's services. Its `createView()` asks the placement's context for a panel view. Modules cannot make one: `ItemContext` gains nothing.
- **A panel sees its own task and group.** The bridge is made for one placement. `currentTask()` is the item's task, `itemStatus()` answers only for items of that task, and `groupTasks()` only for the task's group. Other ids return an empty object or list. Group overview shows the group of the task that holds the panel.
- **Only `dash:` pages in the app profile.** The app profile's page accepts a navigation only to a `dash:` address, and opens no new windows. A link to anywhere else does nothing. So no page except a panel can ever hold the channel.
- **Web rendering.** `rendering()` is `Web`, so the [memory policy](../web-views.md#memory-policy) unloads hidden panels like other pages. `hasStatus()` is false: a panel reports nothing of its own.

## Alternatives

- **A type per panel**, such as `dash.task-summary`. The type picker would show two entries, but every new panel would need a new type, and an item could not change its panel without being made again.
- **A `dash` module in `src/modules/`.** Modules see only the module interface, so the bridge would need new members in `ItemContext` that every module could call, such as reading other tasks. That widens the interface for the sake of one core type.
- **A panel that names any task or group in its configuration.** More flexible, but the configuration would point at ids that can be deleted or be in the trash, and the bridge would expose the whole workspace. The panels in [item types](../item-types.md#built-in-panels) need only their own task and group.

## Consequences

- `ItemTypeRegistry` gains a way to register a built-in module, and `ModulesDialog` its "Built in" section. [ui](../ui.md#modules-window) and [decision 0056](0056-modules-window.md) already expect it.
- `PlacementServices` gains callbacks to open an item and a task, which `MainWindow` sets.
- The bridge cannot serve a future panel that shows the whole workspace. That would need a new decision.
