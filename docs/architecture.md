# Architecture

This document describes the source folders, the main classes and how they work together. It stays at folder level. Item types live in [modules](modules.md), which have their own document. Each milestone gets a detailed code plan just before it is implemented. See the [roadmap](roadmap.md).

## Source folders

```
src/
  main.cpp       registers dash:// before QApplication, registers the enabled modules, wires the folders together
  core/          data model, loading and saving, device settings
  layout/        layout trees and the widgets that show them
  items/         item type interface and registry, module interface, placements
  web/           profiles, web panels, built-in panels
  api/           connections, API client base, status cache, secrets
  platform/      code that differs between Windows, macOS and Linux
  ui/            main window, sidebar, Focus, switcher, dialogs
  modules/       one folder per module of item types; see modules.md
cmake/
  MiranteModule.cmake   mirante_add_module() and the generated registration
resources/
  panels/        HTML, JavaScript and CSS of the built-in panels
tests/           Qt Test
```

Dependencies point downwards only:

```
ui ──► layout ──► items ──► web, api ──► core
 └──────────────────────────► platform

modules/* ──► module interface (mirante::module_api)
main.cpp  ──► generated enabled_modules.cpp ──► modules/*
```

No core folder depends on a module. The core reaches modules only through the registries filled at startup. Core code never names a service such as GitLab or Jira. See [decision 0013](decisions/0013-compiled-in-modules.md).

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
| `ModuleRegistrar` | Receives what each module registers: item types, connection kinds, settings, quick add handlers, related-item finders. See [modules](modules.md#what-a-module-registers). |
| `ItemContext` | What an item's code can reach: settings, connections, status, profile, folders, terminal. See [modules](modules.md#item-context). |
| `UnknownItemType` | Placeholder for an item whose module is not in this build. See [missing modules](modules.md#missing-modules). |
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
| `ConnectionKindRegistry` | Holds the connection kinds registered by modules |
| `ConnectionResolver` | Finds the connection for an item: item, then project, then group. See [connections](data-model.md#connection). |
| `SecretStore` | Reads and writes tokens through qtkeychain, asynchronously |
| `ApiClient` | Shared base for the modules' clients: authentication through the connection kind, ETags, paging, rate limits, at most four parallel requests for each connection |
| `StatusCache` | Polls, caches by connection and request, computes attention, and emits `statusChanged(itemRef)` |
| `Attention` | Collects the attention reasons reported by item types, and feeds badges, Focus, the tray and notifications |

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
| `QuickAdd` | Creates a task from a pasted link, using the modules' quick add handlers. See [quick add](ui.md#quick-add). |
| `DetachedWindow` | A top-level window holding one placement |
| Dialogs | Group, project, task, item, connection and settings editors |

## Data flow

1. `Store` loads files into `Model`.
2. `Sidebar`, `FocusView` and the open `LayoutView`s read from `Model` and listen to its signals.
3. A user edit calls a `Model` method. `Model` emits a signal. `Store` writes the file, and the views update.
4. An external file change makes `Store` reload the file into `Model`, which emits the same signals.
5. Native item views ask the `StatusCache` for data and listen to `statusChanged`.
6. Item types fetch data through the `StatusCache`, which uses `ConnectionResolver`, `SecretStore` and the modules' clients. Item types report attention, which feeds badges, Focus and the tray.

## Ownership and lifetime

- `main.cpp` creates the long-lived services in this order: `DeviceSettings`, the registries (filled by `registerEnabledModules()`), `Model`, `Store`, `SecretStore`, `StatusCache`, `ProfileManager`, `MainWindow`. It destroys them in reverse order.
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
- **Modules:** each module has an option `MIRANTE_MODULE_<ID>`, on by default. `MIRANTE_MODULES` sets the exact list. See [building](modules.md#building).
- **Linux packages:** libsecret development files for qtkeychain, and X11 development files for QHotkey.
- **Deployment:** `windeployqt` on Windows, an app bundle with `macdeployqt` on macOS. Linux builds are not packaged in the first version.

## Tests

Qt Test covers the parts without widgets:

- JSON conversion and migrations of all entities;
- layout tree edits and normalisation;
- connection resolution;
- URL recognition for every item type, in each module's own tests;
- `Store` reload after an external change, and conflict copy detection;
- priority sorting;
- terminal command building for each platform;
- attention rules on recorded API responses, in each module's own tests;
- write-back of items, connections and settings from a missing module, unchanged.

CTest runs the tests of the enabled modules only.

Widgets and web views are checked by hand, following the acceptance criteria in the [roadmap](roadmap.md).
