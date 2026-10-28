# Architecture

This document describes the modules, the main classes and how they work together. It stays at module level. Each milestone gets a detailed code plan just before it is implemented. See the [roadmap](roadmap.md).

## Modules

```
src/
  main.cpp       registers dash:// before QApplication, wires the modules together
  core/          data model, loading and saving, device settings
  layout/        layout trees and the widgets that show them
  items/         item type registry and one class per type
  web/           profiles, web panels, built-in panels
  api/           connections, GitLab and Jira clients, status cache, secrets
  platform/      code that differs between Windows, macOS and Linux
  ui/            main window, sidebar, Focus, switcher, dialogs
resources/
  panels/        HTML, JavaScript and CSS of the built-in panels
tests/           Qt Test
```

Dependencies point downwards only:

```
ui ──► layout ──► items ──► web, api ──► core
 └──────────────────────────► platform
```

`core` depends on Qt Core only. It has no widgets, so it is easy to test.

## core

| Class | Responsibility |
|---|---|
| `Group`, `Project`, `Task`, `Item`, `Dashboard`, `Connection`, `Folder` | Plain value types matching the [data model](data-model.md), with JSON conversion |
| `Model` | Holds all entities, looks them up by id, and emits signals such as `taskChanged(id)` and `taskRemoved(id)` |
| `Store` | Loads the data folder into `Model`, writes changes, watches for external changes, detects conflict copies. See [sync](sync.md). |
| `Migrations` | Upgrades files from older `schemaVersion` values |
| `DeviceSettings` | Typed access to the `QSettings` values listed in [sync](sync.md#device-settings) |
| `Sorting` | Priority and order sorting, used by the sidebar and Focus |

All changes go through `Model` methods. Views never change entities directly. `Store` listens to `Model` signals to write files.

## layout

| Class | Responsibility |
|---|---|
| `LayoutNode` | The [layout tree](data-model.md#layout-tree): split, tabs and cell nodes, with JSON conversion and normalisation |
| `LayoutView` | Builds widgets from a tree: `QSplitter` for splits, `QTabWidget` for tabs, `CellFrame` for cells. It updates the widgets in place when the tree changes, without rebuilding everything. |
| `CellFrame` | Cell header and content. It asks a `PlacementFactory` for the content widget. |
| `DropOverlay` | Shows the five drop zones while dragging, and returns where the drop lands |
| `LayoutEditor` | Turns context menu commands and drops into tree edits, then normalises |
| `Presets` | Builds the starting trees listed in [ui](ui.md#layout-presets) |

The same `LayoutView` shows task dashboards and root dashboards. A task cell inside a root dashboard holds a nested `LayoutView` for the chosen task dashboard. Edits in the nested view change that task dashboard. See [decision 0011](decisions/0011-root-cells-reference-task-dashboards.md).

## items

| Class | Responsibility |
|---|---|
| `ItemType` | Interface: id, name, icon, fields, rendering, connection kind, status, actions, `fromUrl`, `createView` |
| `ItemTypeRegistry` | Holds all types, finds a type by id, and finds the type that recognises a pasted URL |
| One class per type | For example `GitLabMrSummaryType`, `JiraListType`. The full list is in [item types](item-types.md). |
| `Placement` | One shown instance of an item: owns its view widget, and tells the `ViewPool` when it becomes visible or hidden |
| `ViewPool` | Applies the [memory policy](web-views.md#memory-policy) to web views |

Native views are ordinary widgets. Lists use `QTableView` with a model fed from the `StatusCache`.

## web

| Class | Responsibility |
|---|---|
| `ProfileManager` | Creates and owns the group, shared and app profiles, and resolves which profile a URL uses. See [profiles](web-views.md#profiles). |
| `WebPanel` | `QWebEngineView` with a toolbar, popup handling and permission handling |
| `DashSchemeHandler` | Serves `dash://` from resources with the security headers |
| `DashBridge` | The `QWebChannel` object `dash` used by built-in panels |

## api

| Class | Responsibility |
|---|---|
| `ConnectionResolver` | Finds the connection for an item: item, then project, then group. See [connections](data-model.md#connection). |
| `SecretStore` | Reads and writes tokens through qtkeychain, asynchronously |
| `ApiClient` | Shared base: authentication headers, ETags, paging, rate limits, at most four parallel requests for each connection |
| `GitLabClient`, `JiraClient` | Typed requests and parsed results. See [integrations](integrations.md). |
| `StatusCache` | Polls, caches by connection and request, computes attention, and emits `statusChanged(itemRef)` |
| `Attention` | Rules that turn cached data into attention reasons |

## platform

| Class | Responsibility |
|---|---|
| `TerminalLauncher` | Opens a terminal running a command, in a folder. See [terminal launch](integrations.md#terminal-launch). |
| `GlobalHotkey` | Wraps QHotkey. Reports when the platform cannot register a hotkey. |
| `Tray` | `QSystemTrayIcon` with the attention count. It falls back when no tray exists. |
| `GitHead` | Reads the current branch from a folder's `.git/HEAD`, including worktrees |

Code that differs per platform lives only here, behind these classes.

## ui

| Class | Responsibility |
|---|---|
| `MainWindow` | Sidebar, root tab bar, preview tab, toolbar, shortcuts |
| `Sidebar` | Tree model and view of groups, projects and tasks, with drag and drop |
| `FocusView` | The [Focus](ui.md#focus-view) list |
| `QuickSwitcher` | `Ctrl+K` fuzzy search |
| `QuickAdd` | Creates a task from a pasted link. See [quick add](ui.md#quick-add). |
| `DetachedWindow` | A top-level window holding one placement |
| Dialogs | Group, project, task, item, connection and settings editors |

## Data flow

1. `Store` loads files into `Model`.
2. `Sidebar`, `FocusView` and the open `LayoutView`s read from `Model` and listen to its signals.
3. A user edit calls a `Model` method. `Model` emits a signal. `Store` writes the file, and the views update.
4. An external file change makes `Store` reload the file into `Model`, which emits the same signals.
5. Native item views ask the `StatusCache` for data and listen to `statusChanged`.
6. `StatusCache` uses `ConnectionResolver`, `SecretStore` and the clients to fetch data. It then updates attention, which feeds badges, Focus and the tray.

## Ownership and lifetime

- `main.cpp` creates the long-lived services in this order: `DeviceSettings`, `Model`, `Store`, `SecretStore`, `StatusCache`, `ProfileManager`, `ItemTypeRegistry`, `MainWindow`. It destroys them in reverse order.
- Each `Placement` owns its view widget.
- `ProfileManager` owns the profiles and is destroyed after all web views.

## Threading

Everything runs on the main thread:

- network calls are asynchronous through `QNetworkAccessManager`;
- the files are small, so reading and writing them is synchronous;
- keychain calls are asynchronous jobs.

A worker thread is added only if a measured problem needs one.

## Build

- CMake 3.21 or newer.
- `find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets WebEngineWidgets WebChannel Network)`. `Qt6::Test` is used for tests.
- qtkeychain and QHotkey come through `FetchContent`, built for Qt 6.
- **Compilers:** MSVC 2022 64-bit on Windows, because Qt WebEngine does not support MinGW. Clang on macOS. GCC on Linux.
- **Linux packages:** libsecret development files for qtkeychain, and X11 development files for QHotkey.
- **Deployment:** `windeployqt` on Windows, an app bundle with `macdeployqt` on macOS. Linux builds are not packaged in the first version.

## Tests

Qt Test covers the parts without widgets:

- JSON conversion and migrations of all entities;
- layout tree edits and normalisation;
- connection resolution;
- URL recognition for every item type;
- `Store` reload after an external change, and conflict copy detection;
- priority sorting;
- terminal command building for each platform;
- attention rules on recorded API responses.

Widgets and web views are checked by hand, following the acceptance criteria in the [roadmap](roadmap.md).
