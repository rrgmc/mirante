# Architecture

This document describes the source folders, the main classes and how they work together. It stays at folder level. Item types live in [modules](modules.md), which have their own document. Each milestone gets a detailed code plan just before it is implemented. See the [roadmap](roadmap.md).

## Source folders

```
src/
  main.cpp       registers dash:// before QApplication, installs the language, registers the enabled modules, wires the folders together
  core/          data model, layout trees, loading and saving, device settings
  layout/        the widgets that show and edit layout trees
  items/         item type interface and registry, module interface, placements
  web/           profiles, web panels, built-in panels
  api/           API client base, status cache, secrets
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
| `Group`, `Project`, `Task`, `Item`, `Dashboard`, `Folder`, `Profile` | Plain value types matching the [data model](data-model.md), with JSON conversion |
| `Model` | Holds all entities and the workspace's name and profiles, with their module settings, looks them up by id, and emits signals such as `taskChanged(id)` and `taskRemoved(id)` |
| `Store` | Loads the data folder into `Model`. It turns each model signal into a write of one file through `FileWriter`, applies the changes `FolderWatcher` reports through the model's setters, deletes image files whose last reference a local edit removed, and lists conflict copies. A resource folder, such as the sample data, opens read-only. See [sync](sync.md). |
| `FileWriter` | Writes and deletes files in the data folder with `QSaveFile`, debounced by 500 ms, and remembers a hash of what it wrote so its own writes can be told apart |
| `FolderWatcher` | Watches the top of the data folder and its five subfolders, and reports files added, changed or removed by others, after 300 ms of quiet, and the conflict copies present |
| `Migrations` | Upgrades files from older `schemaVersion` values, and refuses files of a newer one |
| `ConflictNames` | Recognises a sync tool's conflict copy by the id at the start of its name. See [decision 0031](decisions/0031-conflict-copies-by-id-prefix.md). |
| `Profiles` | Finds the profile of an item by the [resolution order](data-model.md#profile), lists the profiles to offer, and removes one from the list |
| `ProfileMigration` | Turns connections, of groups or profiles, into module settings, and moves the workspace's module settings into the profile with the id `default`. See [migration to profiles](data-model.md#migration-to-profiles). |
| `Edits` | Changes to entities as pure functions, such as removing an item from a task and clearing its cells, for the `Model`'s setters |
| `DeviceSettings` | Typed access to the `QSettings` values listed in [sync](sync.md#device-settings), including the modules' device settings |
| `GitHead` | Reads the current branch from a folder's `.git/HEAD`, following a worktree's `.git` file, without starting a process. See [decision 0080](decisions/0080-git-read-by-the-core.md). |
| `GitStatusReader` | Runs `git status --porcelain=v2 --branch` on a folder in the background, one run per folder at a time, and parses its output |
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
| `DashboardCellFactory` | Makes the cells of real dashboards. An item cell gets a `Placement` as its body when `LayoutServices::placements` is set, and a maximise button when `LayoutServices::toggleMaximised` is set. The cell of an item whose type has a `pastedImageField` takes images dropped from other apps when `LayoutServices::readImageDrop` and `dropImage` are set. |
| `DropOverlay` | Shows the five drop zones while dragging, and returns where the drop lands |
| `LayoutEditor` | Turns context menu commands and drops into tree edits, then normalises |
| `Presets` | Builds the starting trees listed in [ui](ui.md#layout-presets) |

The same `LayoutView` shows task dashboards and root dashboards. A task cell inside a root dashboard holds a nested `LayoutView` for the chosen task dashboard. Edits in the nested view change that task dashboard. See [decision 0011](decisions/0011-root-cells-reference-task-dashboards.md).

## items

| Class | Responsibility |
|---|---|
| `ItemType` | Interface: id, name, icon, fields, rendering, status, actions, `fromUrl`, `createView`, and the view flags `singleView` and `reparentSafe`. See [the view](item-types.md#the-view). |
| `ItemTypeRegistry` | Holds all types, finds a type by id, and finds the type that recognises a pasted URL. It knows which module is built in. It also holds each module's name, settings screens, device settings, quick add handlers and related-item finders, and gives each module a `ModuleSettings` that reads the model through a reader set in `main.cpp`. |
| `ModuleRegistrar` | Receives what each module registers: item types, its name, settings screens, settings, quick add handlers, related-item finders. See [modules](modules.md#what-a-module-registers). |
| `ItemContext` | What an item's code can reach: settings, module settings, status, profile, folders, terminal. See [modules](modules.md#item-context). |
| `UnknownItemType` | Placeholder for an item whose module is not in this build. See [missing modules](modules.md#missing-modules). |
| `Placement` | One shown instance of an item, and the body of its cell. It makes the view with the type's `createView` when it is first shown, and owns it. It makes the view again when the item's type, configuration or profile changes, or the module settings of that profile, and drops it when the item goes. A web view is added to the `ViewPool`, and the placement tells the pool when it is shown or hidden. It reports the view's header widget, such as a web toolbar, with `headerWidgetChanged`. It also shows the page of `ItemContext::showPage()` in place of the view, with a "Back to summary" button in the header widget, as in [decision 0069](decisions/0069-open-page-in-the-placement.md). |
| `QuickAddRun` | The work of quick add without widgets: runs the modules' handlers and finders asynchronously, matches a proposal to a project, and creates the task with its items and dashboard, or adds the items to an existing task without changing its dashboards. For a task, it leaves out items the task already has. |
| `ItemContextImpl` | The `ItemContext` of one placement. It reads the item, its project and its module settings from the `Model` on each call, folders from `DeviceSettings`, and web logins from the `ProfileManager`. `createWebView` makes a `WebPanel` and reports its toolbar to the placement. Without a `ProfileManager`, as in tests, it returns a message instead of a page. Git state comes from `GitHead` and `GitStatusReader`, and programs start through `Launcher`. |
| `DashBridge` | The `QWebChannel` object `dash` of one built-in panel: its task and group, ready to show, and the links that open them. It is in this library, not in `web`, because it reads the status cache and attention from `api`. See [built-in panels](web-views.md#built-in-panels). |
| `dash::registerModule` | Registers the core's module `dash`, "Built in", with the item type `dash.panel`, in `DashPanel.cpp`. Its views come from `ItemContextImpl::createDashPanel()`, which modules cannot reach. See [decision 0094](decisions/0094-built-in-panels-as-a-core-item-type.md). |
| `ViewPool` | Applies the [memory policy](web-views.md#memory-policy) to web views. When more hidden views are loaded than the limit, it asks the placements hidden longest to unload. |

Native views are ordinary widgets. Lists use `QTableView` with a model fed from the `StatusCache`.

## web

| Class | Responsibility |
|---|---|
| `SingleInstance` | Keeps one process per [web data folder](README.md#glossary). It holds a `QLockFile` on `.lock` in the folder, and listens on a local socket named after it. A later start sends its `--data` folder there and exits; the holder emits `activationRequested`. See [decision 0058](decisions/0058-one-instance-per-web-data-folder.md). |
| `ProfileManager` | Creates and owns the `QWebEngineProfile`s that keep web logins: one for each profile id, and the app profile. It deletes a profile's logins, and the folders of the profiles from before decision 0042. Which profile an item uses is decided by `profiles::effective` in the core. See [profiles](web-views.md#profiles) and [decision 0042](decisions/0042-user-managed-web-profiles.md). |
| `WebPanel` | `QWebEngineView` with a toolbar, popup handling and permission handling |
| `WebPage` | The page of a web panel or login dialog: new windows and permissions |
| `PageArea` | A page with the permission bar and the load failure message |
| `LoginDialog` | The dialog for a login popup, in the opener's profile |
| `AddressField` | The elided, selectable address in the web toolbar |
| `DashSchemeHandler` | Serves `dash://` from resources with the security headers. Only the app profile installs it. |
| `DashPage`, `DashView` | The page and view of a built-in panel: only `dash:` addresses, no new windows, no permissions, and the bridge on a `QWebChannel` as `dash` |

## api

| Class | Responsibility |
|---|---|
| `ModuleDirectory` | What the library needs to know about the modules: their types, servers, settings and screens. `ItemTypeRegistry` implements it, so `api` does not depend on `items`. |
| `ApiServices` | The library's long-lived services, handed from `main.cpp` to the window |
| `SecretStore` | Reads and writes tokens through qtkeychain, asynchronously: those of the settings screens' `Secret` fields, under `<workspace id>/<profile id>/<module id>/<field key>`. See [servers and tokens](data-model.md#servers-and-tokens). |
| `ServerClients` | The modules' `ApiClient`s, one for each profile and module with a `ServerType`: authentication with the profile's token, ETags, paging, rate limits, at most four parallel requests for each server of a profile, and pausing while a token is refused |
| `StatusCache` | Polls, caches by profile, module and request, computes attention, and emits `statusChanged(itemRef)` |
| `Attention` | Collects the attention reasons reported by item types, and feeds badges, Focus, the tray and notifications |

## platform

| Class | Responsibility |
|---|---|
| `Launcher` | Builds and starts the command that opens a terminal running a program in a folder, starts a program such as an editor, and shows a folder in the file manager. A program and its arguments are passed apart and quoted for each platform. See [terminal launch](integrations.md#terminal-launch) and [decision 0082](decisions/0082-commands-as-argument-lists.md). |
| `GlobalHotkey` | Wraps QHotkey. Reports when the platform cannot register a hotkey. |
| `Tray` | The tray icon and its menu: `QSystemTrayIcon`, or a native status item on macOS. The attention count arrives in M8. It reports when no tray exists, so the window can fall back. |
| `Foreground` | `allowForegroundFor(pid)`: on Windows, lets another process bring its window to the front. A later start calls it for the running instance before it hands over. |

Code that differs per platform lives only here, behind these classes.

## ui

| Class | Responsibility |
|---|---|
| `MainWindow` | Sidebar, root tab bar, the Focus and preview slots (`SlotTabBar`), warning and conflict bars, toolbar, shortcuts. It opens workspaces through the `Store`, and saves and restores each workspace's open tabs through `DeviceSettings`. |
| `Sidebar` | Tree model and view of groups, projects and tasks, with drag and drop |
| `EntityActions` | Every create, edit, move and delete command for groups, projects, tasks, items and task dashboards, shared by the sidebar menus, the menu bar and sidebar drops |
| `FocusView` | The [Focus](ui.md#focus-view) list |
| `ConflictBar` | One line for each conflict copy, with "Keep current", "Use other" and "Compare". See [sync](sync.md#conflict-copies). |
| `ImagePaste` | Pastes an image from the clipboard into an image item, or as a new item of a task, on no dashboard. See [image](item-types.md#image). |
| `ImagePasteDialog` | The paste dialog: a preview of a pasted image, with an `ImageCropView` to crop it. See [pasting images](ui.md#pasting-images). |
| `QuickSwitcher` | `Ctrl+K` fuzzy search |
| `QuickAddDialog` | The [quick add](ui.md#quick-add) dialog. It shows what a `QuickAddRun` proposes and finds, and creates the task, "New Task from Link". Opened for a task, it adds the items to that task, "Add Items from Link". See decisions [0078](decisions/0078-quick-add-dialog.md) and [0083](decisions/0083-quick-add-into-a-task.md). |
| `DashboardPage` | One tab's or slot's page: a `LayoutView` of one dashboard with its cell factory, or the Focus list. A detached window holds one too. |
| `DetachedWindow` | A top-level window holding one detached root dashboard. See [decision 0024](decisions/0024-detach-root-dashboard-tabs.md). |
| `SettingsDialog` | The [settings window](ui.md#settings): options of this device, kept in `DeviceSettings`, in sections. Each change is saved at once and reported with `changed()`. |
| `ModulesDialog` | The [Modules window](ui.md#modules-window), from Help > Modules…: the modules of the build and their item types, with their descriptions, read from `ItemTypeRegistry`. Read-only. |
| Dialogs | Group, project, task, item and settings editors, the [profiles](ui.md#profiles) list, and the dialogs of module [settings screens](ui.md#modules) |

## Data flow

1. `Store` loads files into `Model`.
2. `Sidebar`, `FocusView` and the open `LayoutView`s read from `Model` and listen to its signals.
3. A user edit calls a `Model` method. `Model` emits a signal. `Store` writes the file, and the views update.
4. An external file change makes `Store` reload the file into `Model`, which emits the same signals.
5. Native item views ask the `StatusCache` for data and listen to `statusChanged`.
6. Item types fetch data through the `StatusCache`, which uses the item's profile, `SecretStore` and the modules' clients. Item types report attention, which feeds badges, Focus and the tray.

## Ownership and lifetime

- Before any service, `main.cpp` reads the command line and makes a `SingleInstance` for the web data folder. If another process holds its lock, `main.cpp` hands `--data` over to it and exits, so no settings, model or profiles are made. The `SingleInstance` lives until the end of `main`, so the lock is released only after the `ProfileManager` is gone. After the window is shown, `main.cpp` connects `activationRequested` to the window: it opens the given workspace, if any, and brings the window to the front. See [decision 0058](decisions/0058-one-instance-per-web-data-folder.md).
- Right after the command line, `main.cpp` makes the `DeviceSettings`, from `--settings` if given, and installs the language before any text is made: `language::choose()` turns the `language` setting and the system's languages into `en` or `pt_BR`, `QLocale::setDefault()` makes dates and numbers follow it, and two `QTranslator`s load Mirante's translation from `:/i18n/mirante_<language>.qm` and, for Portuguese, Qt's `qtbase` translation from `QLibraryInfo::TranslationsPath`. English loads Mirante's file too, for its plural forms. Tests install no translator and use the English locale. See [decision 0086](decisions/0086-english-and-brazilian-portuguese.md).
- `main.cpp` creates the long-lived services in this order: `DeviceSettings`, the registries (filled by `registerEnabledModules()`), `Model`, `Store`, `SecretStore`, `ServerClients`, `StatusCache`, `Attention`, `ProfileManager`, `ViewPool`, `MainWindow`. It destroys them in reverse order. Before the `QApplication` it sets `Qt::AA_ShareOpenGLContexts`, which Qt WebEngine needs. `DeviceSettings` is created after the application and organisation names are set, because they choose where `QSettings` keeps its values. The `Store` flushes its pending writes when the application is about to quit.
- Each `Placement` owns its view widget and its `ItemContextImpl`. The context outlives the view: the placement deletes its views before its context. A view that is replaced is deleted later, not at once, because the change may come from the view's own code, for example through `updateConfig`.
- A web view's toolbar is owned by its `WebPanel`, even while the cell header shows it. The `CellFrame` only shows it, and lets it go when the placement reports a new header widget.
- `ProfileManager` owns the profiles and is destroyed after all web views. Its root folder is the web data folder: `QStandardPaths::AppLocalDataLocation` + `/profiles`, or the folder given with `--web-data`. `MainWindow` passes it and the `ViewPool` to placements through `PlacementServices`, which `MainWindow` owns. Without a `ProfileManager`, as in most tests, item cells keep their placeholders.
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
- `find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets WebEngineWidgets WebChannel Network LinguistTools)`. `Qt6::Test` is used for tests.
- **Translations:** `qt_add_translations()` in `src/CMakeLists.txt` compiles `translations/mirante_pt_BR.ts` and the English plural forms, `mirante_en.ts`, into the app's resources under `:/i18n`. `lupdate` reads every source file under `src/`, modules included, also in a build without them, so the files stay complete. The `update_translations` target, which `task translations` runs, updates them. See [translations/README.md](../translations/README.md).
- qtkeychain and QHotkey come through `FetchContent`, built for Qt 6.
- **Compilers:** MSVC 2022 64-bit on Windows, because Qt WebEngine does not support MinGW. Clang on macOS. GCC on Linux.
- **Debug builds** link Qt WebEngine's debug libraries, which stop some real sites, such as claude.ai, on Chromium's internal checks. Check web pages with a release build. See [debug builds](web-views.md#debug-builds).
- **Modules:** each module has an option `MIRANTE_MODULE_<ID>`, on by default. `MIRANTE_MODULES` sets the exact list. See [building](modules.md#building).
- **Linux packages:** libsecret development files for qtkeychain, and X11 development files for QHotkey.
- **Deployment:** `windeployqt` on Windows, an app bundle with `macdeployqt` on macOS. Every build runs this step after linking the app, into `<build folder>/bin`, so the build starts on its own. See `cmake/MiranteDeploy.cmake`.
- **Packages:** `cmake --install --component Mirante` makes a self-contained app with Qt's deploy script, and `cpack` makes an NSIS setup on Windows, a disk image on macOS and a `.deb` package on Linux. The version comes from `MIRANTE_VERSION`. See `cmake/MiranteInstall.cmake`, [releasing](releasing.md) and decisions [0098](decisions/0098-packages-from-cmake-install.md), [0099](decisions/0099-linux-package-in-opt.md) and [0101](decisions/0101-macos-release-signing.md).

## Tests

Qt Test covers the parts without widgets:

- JSON conversion and migrations of all entities;
- layout tree edits and normalisation;
- profile resolution, and the migration of connections into module settings;
- URL recognition for every item type, in each module's own tests;
- `Store` reload after an external change, and conflict copy detection;
- priority sorting;
- terminal command building for each platform;
- attention rules on recorded API responses, in each module's own tests;
- write-back of items and settings from a missing module, unchanged.

CTest runs the tests of the enabled modules only.

Widgets and web views are checked by hand, following the acceptance criteria in the [roadmap](roadmap.md).
