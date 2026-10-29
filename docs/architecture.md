# Architecture

This document describes the source folders, the main classes and how they work together. It stays at folder level. Item types live in [modules](modules.md), which have their own document. Each milestone gets a detailed code plan just before it is implemented. See the [roadmap](roadmap.md).

## Source folders

```
src/
  main.cpp       registers dash:// before QApplication, registers the enabled modules, wires the folders together
  core/          data model, layout trees, loading and saving, device settings
  layout/        the widgets that show and edit layout trees
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
| `Group`, `Project`, `Task`, `Item`, `Dashboard`, `Connection`, `Folder`, `Profile` | Plain value types matching the [data model](data-model.md), with JSON conversion |
| `Model` | Holds all entities and the workspace's name and profiles, with their connections and module settings, looks them up by id, and emits signals such as `taskChanged(id)` and `taskRemoved(id)` |
| `Store` | Loads the data folder into `Model`. It turns each model signal into a write of one file through `FileWriter`, applies the changes `FolderWatcher` reports through the model's setters, deletes image files whose last reference a local edit removed, and lists conflict copies. A resource folder, such as the sample data, opens read-only. See [sync](sync.md). |
| `FileWriter` | Writes and deletes files in the data folder with `QSaveFile`, debounced by 500 ms, and remembers a hash of what it wrote so its own writes can be told apart |
| `FolderWatcher` | Watches the top of the data folder and its five subfolders, and reports files added, changed or removed by others, after 300 ms of quiet, and the conflict copies present |
| `Migrations` | Upgrades files from older `schemaVersion` values, and refuses files of a newer one |
| `ConflictNames` | Recognises a sync tool's conflict copy by the id at the start of its name. See [decision 0031](decisions/0031-conflict-copies-by-id-prefix.md). |
| `Connections` | Finds the connection for an item by the [resolution order](data-model.md#connection). Used by `ItemContext` from M4, and by `ConnectionResolver` from M5. |
| `Profiles` | Finds the profile of an item by the [resolution order](data-model.md#profile), lists the profiles to offer, and removes one from the list |
| `ProfileMigration` | Moves groups' connections and the workspace's module settings into profiles. See [migration to profiles](data-model.md#migration-to-profiles). |
| `Edits` | Changes to entities as pure functions, such as removing an item from a task and clearing its cells, for the `Model`'s setters |
| `DeviceSettings` | Typed access to the `QSettings` values listed in [sync](sync.md#device-settings) |
| `GroupColours` | The colour of each group: its own, or a palette colour chosen by its id, and the colour for a new group. See [decision 0046](decisions/0046-group-colours.md). |
| `Sorting` | Priority and order sorting, used by the sidebar and Focus |
| `LayoutTree` | The [layout tree](data-model.md#layout-tree): split, tabs and cell nodes, with JSON conversion, edit operations and normalisation. It lives in `core` because `Dashboard` holds one. |
| `FuzzyMatch` | Scores text against a query for the quick switcher |

All changes go through `Model` methods. Views never change entities directly. `Store` listens to `Model` signals to write files.

## layout

| Class | Responsibility |
|---|---|
| `LayoutView` | Builds widgets from a tree: `QSplitter` for splits, `QTabWidget` for tabs, `CellFrame` for cells. It updates the widgets in place when the tree changes, without rebuilding everything. After the whole view moves to another window, `remakeUnsafeCells()` makes again the cells whose type is not `reparentSafe`. |
| `CellFrame` | Cell header and body. The header holds the type pill, the item name, a widget of the view such as a web toolbar, the menu button and the maximise button. The body is a placeholder until `DashboardCellFactory` gives it a `Placement`. |
| `DashboardCellFactory` | Makes the cells of real dashboards. An item cell gets a `Placement` as its body when `LayoutServices::placements` is set, and a maximise button when `LayoutServices::toggleMaximised` is set. |
| `DropOverlay` | Shows the five drop zones while dragging, and returns where the drop lands |
| `LayoutEditor` | Turns context menu commands and drops into tree edits, then normalises |
| `Presets` | Builds the starting trees listed in [ui](ui.md#layout-presets) |

The same `LayoutView` shows task dashboards and root dashboards. A task cell inside a root dashboard holds a nested `LayoutView` for the chosen task dashboard. Edits in the nested view change that task dashboard. See [decision 0011](decisions/0011-root-cells-reference-task-dashboards.md).

## items

| Class | Responsibility |
|---|---|
| `ItemType` | Interface: id, name, icon, fields, rendering, connection kind, status, actions, `fromUrl`, `createView`, and the view flags `singleView` and `reparentSafe`. See [the view](item-types.md#the-view). |
| `ItemTypeRegistry` | Holds all types, finds a type by id, and finds the type that recognises a pasted URL. It also holds each module's name and settings screens, and gives each module a `ModuleSettings` that reads the model through a reader set in `main.cpp`. |
| `ModuleRegistrar` | Receives what each module registers: item types, its name, settings screens, connection kinds, settings, quick add handlers, related-item finders. See [modules](modules.md#what-a-module-registers). |
| `ItemContext` | What an item's code can reach: settings, connections, status, profile, folders, terminal. See [modules](modules.md#item-context). |
| `UnknownItemType` | Placeholder for an item whose module is not in this build. See [missing modules](modules.md#missing-modules). |
| `Placement` | One shown instance of an item, and the body of its cell. It makes the view with the type's `createView` when it is first shown, and owns it. It makes the view again when the item's type, configuration or profile changes, or the connections and module settings of that profile, and drops it when the item goes. A web view is added to the `ViewPool`, and the placement tells the pool when it is shown or hidden. It reports the view's header widget, such as a web toolbar, with `headerWidgetChanged`. |
| `ItemContextImpl` | The `ItemContext` of one placement. It reads the item, its project, its connection and its module settings from the `Model` on each call, folders from `DeviceSettings`, and web logins from the `ProfileManager`. `createWebView` makes a `WebPanel` and reports its toolbar to the placement. Without a `ProfileManager`, as in tests, it returns a message instead of a page. |
| `ViewPool` | Applies the [memory policy](web-views.md#memory-policy) to web views. When more hidden views are loaded than the limit, it asks the placements hidden longest to unload. |

Native views are ordinary widgets. Lists use `QTableView` with a model fed from the `StatusCache`.

## web

| Class | Responsibility |
|---|---|
| `ProfileManager` | Creates and owns the `QWebEngineProfile`s that keep web logins: one for each profile id, and the app profile. It deletes a profile's logins, and the folders of the profiles from before decision 0042. Which profile an item uses is decided by `profiles::effective` in the core. See [profiles](web-views.md#profiles) and [decision 0042](decisions/0042-user-managed-web-profiles.md). |
| `WebPanel` | `QWebEngineView` with a toolbar, popup handling and permission handling |
| `WebPage` | The page of a web panel or login dialog: new windows and permissions |
| `PageArea` | A page with the permission bar and the load failure message |
| `LoginDialog` | The dialog for a login popup, in the opener's profile |
| `AddressField` | The elided, selectable address in the web toolbar |
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
| `MainWindow` | Sidebar, root tab bar, preview tab, the Focus tab, warning and conflict bars, toolbar, shortcuts. It opens workspaces through the `Store`, and saves and restores each workspace's open tabs through `DeviceSettings`. |
| `Sidebar` | Tree model and view of groups, projects and tasks, with drag and drop |
| `EntityActions` | Every create, edit, move and delete command for groups, projects, tasks, items and task dashboards, shared by the sidebar menus, the menu bar and sidebar drops |
| `FocusView` | The [Focus](ui.md#focus-view) list |
| `ConflictBar` | One line for each conflict copy, with "Keep current", "Use other" and "Compare". See [sync](sync.md#conflict-copies). |
| `ImagePaste` | Pastes an image from the clipboard into an image item, or as a new one. See [image](item-types.md#image). |
| `ImagePasteDialog` | The paste dialog: a preview of a pasted image, with an `ImageCropView` to crop it. See [pasting images](ui.md#pasting-images). |
| `QuickSwitcher` | `Ctrl+K` fuzzy search |
| `QuickAdd` | Creates a task from a pasted link, using the modules' quick add handlers. See [quick add](ui.md#quick-add). |
| `DashboardPage` | One tab's page: a `LayoutView` of one dashboard with its cell factory, or the Focus list. A detached window holds one too. |
| `DetachedWindow` | A top-level window holding one detached root dashboard. See [decision 0024](decisions/0024-detach-root-dashboard-tabs.md). |
| Dialogs | Group, project, task, item, connection and settings editors, the [profiles](ui.md#profiles) list and its connections dialog, and the dialogs of module [settings screens](ui.md#modules) |

## Data flow

1. `Store` loads files into `Model`.
2. `Sidebar`, `FocusView` and the open `LayoutView`s read from `Model` and listen to its signals.
3. A user edit calls a `Model` method. `Model` emits a signal. `Store` writes the file, and the views update.
4. An external file change makes `Store` reload the file into `Model`, which emits the same signals.
5. Native item views ask the `StatusCache` for data and listen to `statusChanged`.
6. Item types fetch data through the `StatusCache`, which uses `ConnectionResolver`, `SecretStore` and the modules' clients. Item types report attention, which feeds badges, Focus and the tray.

## Ownership and lifetime

- `main.cpp` creates the long-lived services in this order: `DeviceSettings`, the registries (filled by `registerEnabledModules()`), `Model`, `Store`, `SecretStore`, `StatusCache`, `ProfileManager`, `ViewPool`, `MainWindow`. It destroys them in reverse order. `SecretStore` and `StatusCache` come in later milestones. Before the `QApplication` it sets `Qt::AA_ShareOpenGLContexts`, which Qt WebEngine needs. `DeviceSettings` is created after the application and organisation names are set, because they choose where `QSettings` keeps its values. The `Store` flushes its pending writes when the application is about to quit.
- Each `Placement` owns its view widget and its `ItemContextImpl`. The context outlives the view: the placement deletes its views before its context. A view that is replaced is deleted later, not at once, because the change may come from the view's own code, for example through `updateConfig`.
- A web view's toolbar is owned by its `WebPanel`, even while the cell header shows it. The `CellFrame` only shows it, and lets it go when the placement reports a new header widget.
- `ProfileManager` owns the profiles and is destroyed after all web views. Its root folder is `QStandardPaths::AppLocalDataLocation` + `/profiles`. `MainWindow` passes it and the `ViewPool` to placements through `PlacementServices`, which `MainWindow` owns. Without a `ProfileManager`, as in most tests, item cells keep their placeholders.
- The `ViewPool` reads its limit from `DeviceSettings::hiddenViewLimit()` once, at start. Placements remove themselves from the pool when they go, so the pool outlives the window.
- Deleting a profile in `ProfilesDialog` makes `EntityActions` call `ProfileManager::removeProfile` after the model drops the profile, in a queued call. Placements that used it make new views at once, but delete the old ones a moment later, and a profile must outlive its pages. `main.cpp` calls `ProfileManager::removeOldProfiles` at start.

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
