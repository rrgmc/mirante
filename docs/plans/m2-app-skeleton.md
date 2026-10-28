# M2 code plan: app skeleton and layouts

This plan implements [milestone M2](../roadmap.md#m2-app-skeleton-and-layouts). M2 builds the base of the app, so the code is written to last. See [decision 0015](../decisions/0015-app-skeleton-instead-of-spike.md).

M2 reads a fixed data folder and writes nothing. Layout edits, pinned tabs and new root dashboards live in memory until the app quits. There are no web views, no API calls and no device settings.

## Changes to the architecture

- **The layout tree moves to `core`.** `Dashboard`, in `core`, holds a layout tree, and `core` cannot depend on `layout`. So `LayoutNode`, its JSON conversion, its edit operations and normalisation move to `core/LayoutTree`. They need Qt Core only, so they stay easy to test. `layout/` keeps the widgets: `LayoutView`, `CellFrame`, `DropOverlay`, `LayoutEditor` and `Presets`. [architecture.md](../architecture.md) is updated in the same commit.
- **Qt modules are added when first used.** M2 needs Core, Gui, Widgets and Test. Network joins in M5, and WebEngineWidgets and WebChannel in M4. This keeps the CI install small until then.

## Source layout

```
CMakeLists.txt                top level: project, options, subfolders, registration
CMakePresets.json             configure, build and test presets
cmake/
  MiranteModule.cmake         mirante_add_module(), mirante_finish_modules()
  enabled_modules.cpp.in      template for the generated registration
  Dependencies.cmake          FetchContent for qtkeychain and QHotkey
.github/workflows/ci.yml
src/
  main.cpp
  core/          mirante_core: entities, Model, Store (read only), Sorting, LayoutTree, FuzzyMatch
  module_api/    mirante::module_api: the headers modules may include
    mirante/Module.h  mirante/ItemType.h
  items/         mirante_items: ItemTypeRegistry, ModuleRegistrar, UnknownItemType, Placement
  layout/        mirante_layout: LayoutView, CellFrame, TaskCell, DropOverlay, LayoutEditor
  ui/            mirante_ui: MainWindow, Sidebar, RootTabBar, QuickSwitcher, Shortcuts
  platform/      mirante_platform: empty in M2 apart from linking qtkeychain and QHotkey
  modules/
    claude/ folder/ gitlab/ jira/ notes/ web/     one stub module each
resources/
  fixtures/      the fixture data folder, compiled in as a resource
tests/
  core/          tests for core
  items/         tests for the registry and missing modules
```

Each folder in `src/` is one static library. The executable `mirante` links them all, plus the enabled modules. Dependencies follow [architecture](../architecture.md#source-folders): `ui → layout → items → core`, with `platform` used by `ui` only.

## Part 1: build skeleton and CI

**CMake.**

- CMake 3.21, C++17, `CMAKE_AUTOMOC` on.
- `find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets Test)`, then `qt_standard_project_setup()`.
- Warnings: `/W4 /permissive-` on MSVC, `-Wall -Wextra -Wpedantic` elsewhere. Warnings are errors in CI only, through the option `MIRANTE_WERROR`, so a new compiler version does not break local builds.
- `Dependencies.cmake` fetches qtkeychain and QHotkey with `FetchContent`, each pinned to a release tag, built static and for Qt 6. `mirante_platform` links both, so CI proves that they build on all three platforms. They are not called until M5 and M8.
- `enable_testing()`. Every test is a Qt Test executable added with a helper `mirante_add_test(name SOURCES ... LIBS ...)`, which also sets `QT_QPA_PLATFORM=offscreen` in the test environment.

**Presets.** `CMakePresets.json` uses the Ninja generator and defines:

| Preset | Use |
|---|---|
| `dev` | Debug, all modules |
| `release` | Release, all modules |
| `no-modules` | Debug, `MIRANTE_MODULES` set to an empty list |
| `ci` | Release, all modules, `MIRANTE_WERROR=ON` |
| `ci-no-modules` | `ci` with no modules |

Each preset has a matching build and test preset. On Windows, presets are run from a Developer Command Prompt for MSVC 2022 so Ninja finds the compiler.

**CI.** `.github/workflows/ci.yml` runs on every push and pull request:

- **build** job, matrix `windows-2022`, `macos-14` and `ubuntu-24.04`:
  1. install Qt with `jurplel/install-qt-action`, pinned to the newest 6.8 patch release, with its cache on;
  2. on Linux, install `libsecret-1-dev`, `libx11-dev`, `libxkbcommon-dev` and `libgl1-mesa-dev`;
  3. on Windows, set up the MSVC environment with `ilammy/msvc-dev-cmd`;
  4. configure, build and test with the `ci` preset.
- **no-modules** job, Linux only: configure, build and test with the `ci-no-modules` preset.
- **module-includes** job: fails if any file under `src/modules/` includes a path that starts with `../`, or a core header by name. The build already blocks the include path. This catches relative paths, which the build cannot block.

**Executable.** `main.cpp` creates `QApplication`, the registries, `Model` and `MainWindow`, in the order given in [architecture](../architecture.md#ownership-and-lifetime). Services that do not exist yet are left out. The application name, organisation and version come from CMake through a generated `Version.h`.

**Done when** CI is green on all three platforms with an empty main window, and the commit is merged.

## Part 2: module mechanism and stub modules

**`mirante::module_api`.** An `INTERFACE` target whose only include folder is `src/module_api`. Modules include `<mirante/ItemType.h>`. Core folders use the same headers through the same target. Modules do not get `src/` on their include path, so they cannot include core headers.

In M2 the interface holds two headers, trimmed to what M2 uses. The rest of the headers listed in [modules](../modules.md#the-module-interface) are added in the milestone that first needs them.

```cpp
// mirante/ItemType.h
namespace mirante {

class ItemType {
public:
    virtual ~ItemType() = default;
    virtual QString id() const = 0;            // "gitlab.mr.summary"
    virtual QString name() const = 0;          // "GitLab MR (summary)"
    virtual QIcon icon() const = 0;
    virtual bool singleView() const { return false; }
    virtual bool reparentSafe() const { return true; }
    // createView() is added in M4, with ItemContext. In M2 every cell shows a placeholder.
};

}

// mirante/Module.h
namespace mirante {

class ModuleRegistrar {
public:
    virtual ~ModuleRegistrar() = default;
    virtual void addItemType(std::unique_ptr<ItemType> type) = 0;
};

}
```

`ModuleRegistrar` is abstract, so a module's static library has no link dependency on core code. The implementation lives in `items/`.

**`ItemTypeRegistry`** owns the item types. It implements `ModuleRegistrar` through a small adapter that knows the current module id, and rejects, with a `qFatal`, an id that does not equal the module id or start with it and a dot, and an id registered twice. A mistake there is a programming error in a module, found by its own tests.

- `type(id)` returns the registered type, or an `UnknownItemType` made for that id. `UnknownItemType::moduleId()` is the part of the id before the first dot. It is never null, so callers need no special case.
- `types()` lists the registered types, sorted by name, for later menus.

**`mirante_add_module()`** in `cmake/MiranteModule.cmake` follows [modules](../modules.md#building):

1. Checks that the id has lowercase letters and digits only.
2. Defines `MIRANTE_MODULE_<ID>`. If `MIRANTE_MODULES` is defined, the option is forced on when the id is in the list and off otherwise.
3. If the option is off, returns.
4. Builds `mirante_module_<id>` as a static library, linked to `mirante::module_api`, `Qt6::Core`, `Qt6::Gui` and `Qt6::Widgets` only.
5. Adds `RESOURCES` with `qt_add_resources()` on the module target, under the prefix `/modules/<id>`. Qt 6 links resources of static libraries as object files, so no `Q_INIT_RESOURCE` call is needed. A test checks that each icon loads.
6. Adds each `TESTS` source as a test, linked to the module and to `mirante_items`.
7. Records the id and its `DEPENDS` in global properties.

**`mirante_finish_modules()`**, called by the top level after all module folders:

- fails if `MIRANTE_MODULES` names an id that has no folder;
- fails if an enabled module depends on a disabled one, naming both;
- sorts the enabled ids and generates `enabled_modules.cpp` from `enabled_modules.cpp.in`, with one forward declaration and one call per module, and a function `enabledModuleIds()`;
- adds the generated file and the module libraries to the `mirante` executable.

With no modules enabled, the generated function is empty and the app still runs. Every item then shows the missing-module placeholder.

**Stub modules.** Each of the six modules in [modules](../modules.md#the-modules) gets its folder with:

- `CMakeLists.txt`;
- `README.md`;
- `<Name>Module.cpp`, registering every item type listed for the module, through one small `StubItemType` class per module that holds id, name and icon;
- one SVG icon per module, used by all its types for now;
- `tests/tst_register.cpp`, which registers the module into a fresh registry and checks its ids and that its icon is not null.

**Done when** the `dev` and `no-modules` presets both build and pass their tests, and CI runs both.

## Part 3: core

**Entities.** `Group`, `Project`, `Folder`, `Task`, `Item`, `Dashboard`, `Connection` are plain structs with `fromJson()` and `toJson()`, matching the [data model](../data-model.md).

- Ids are `QString`s holding UUIDs. Using strings, not `QUuid`, keeps write-back byte-exact.
- Fields the core does not interpret stay `QJsonObject`: `Item::config`, `Connection::fields`, `Project::moduleSettings`, `Group::profileOverrides`.
- Every struct has `QJsonObject extra`. `fromJson()` moves every key it does not know into it, and `toJson()` writes it back. This is how unknown fields and fields of newer versions survive, as [sync](../sync.md#the-data-folder) requires.
- Data of missing modules needs no special code: the core keeps `type`, `config`, connection kinds and module settings as opaque values whether or not the module is in the build.
- `fromJson()` returns `std::optional` and an error text for files that are not usable, such as a missing `id`. Wrong types in optional fields fall back to their default and keep the original value in `extra`, so nothing is lost.

**`LayoutTree`** in `core/LayoutTree.h`:

```cpp
struct CellRef { QString item, task, dashboard; };   // empty strings mean "not set"

struct LayoutNode {
    enum class Kind { Split, Tabs, Cell } kind = Kind::Cell;
    // Split
    Qt::Orientation dir = Qt::Horizontal;
    std::vector<double> sizes;
    // Split and Tabs: children; Tabs also titles and the current index
    std::vector<LayoutNode> children;
    std::vector<QString> titles;
    int current = 0;
    bool collapseSingle = false;
    // Cell
    std::optional<CellRef> ref;   // nullopt is an empty cell
    bool removed = false;         // set by normalisation, never written
    QJsonObject extra;
};

using NodePath = std::vector<int>;   // child indices from the root
```

- The tree is a value. Edits copy, change and return a new tree, which makes them easy to test and to compare.
- Nodes are addressed by `NodePath`. The M1 prototype gave nodes transient ids. Paths need no bookkeeping, and every edit returns the paths the view needs, such as where a moved cell landed.
- **Edit operations**, all pure functions in `namespace mirante::layout`:
  - `splitCell(tree, path, Edge, CellRef)`;
  - `wrapInTabs(tree, path)`;
  - `addTab(tree, path, CellRef)`;
  - `insertTab(tree, tabsPath, index, CellRef)`;
  - `setRef(tree, path, optional<CellRef>)`;
  - `removeNode(tree, path)`;
  - `moveNode(tree, from, to, Zone)`, where the zone is an edge, the centre or a tab bar position;
  - `renameTab`, `moveTab`, `setCurrentTab`, `setCollapseSingle`;
  - `setSizes(tree, splitPath, sizes)`.
- `normalise(tree, exists)` applies the six [normalisation rules](../data-model.md#normalisation), repeated until nothing changes. `exists` is a function that says whether a `CellRef` still refers to something. A dangling reference becomes an empty cell with `removed` set.
- `sameShape(a, b)` compares two trees, ignoring `sizes` and `current`. `LayoutView` uses it to tell a resize from a structural change.

**`Model`** is a `QObject` that owns all entities.

- Lookups: `group(id)`, `project(id)`, `task(id)`, `item(taskId, itemId)`, `rootDashboard(id)`, `taskOfItem(itemId)`, and the children of each level.
- Changes in M2: `setTaskDashboardLayout(taskId, dashboardId, tree)`, `setRootDashboardLayout(id, tree)` and `addRootDashboard(dashboard)`. Each normalises the tree and emits `taskChanged(id)` or `rootDashboardChanged(id)`. M3 adds the rest.
- `refExists(ref)` implements the `exists` function for normalisation.

**`Store`.** In M2 it has only `load(dir, Model&)`.

- It reads the four subfolders described in [sync](../sync.md#the-data-folder) and accepts `schemaVersion` 1 only.
- A file that cannot be used is skipped and reported in a list of load problems, which the main window shows in its warning bar.
- A second function, `serialise(Model)`, returns the files the model would write. It is used by the write-back test and becomes the base of writing in M3.
- `QJsonDocument` writes object keys in sorted order and `Indented` format gives the pretty-printing that [sync](../sync.md#the-data-folder) asks for.

**`Sorting`.** Sort keys for projects and tasks: priority, then `order`. The Default task comes first. Done and archived tasks can be filtered out. Used by the sidebar now and by Focus in M3.

**`FuzzyMatch`.** A scoring function for the switcher: characters of the query in order, with bonuses for word starts and consecutive matches, case-insensitive. A pure function, tested on its own.

**Fixtures.** `resources/fixtures/` is a data folder with the content of the M1 prototype's mock data, converted to the real file format with fixed UUIDs:

- two groups;
- five projects;
- tasks in all four priorities and three states;
- items of every type in [item types](../item-types.md);
- nested task dashboards, one task with two dashboards;
- two root dashboards.

It also holds data of a module that does not exist, `timetrack`: an item `timetrack.entry`, a connection of kind `timetrack`, and `moduleSettings.timetrack` on a project. Some files carry unknown fields.

The conversion is done once with a small script, which is not committed. The fixture folder is compiled in as a resource. The app loads it unless started with `--data <folder>`.

**Tests**, in `tests/core/`:

| Test | Covers |
|---|---|
| `tst_entities` | `fromJson` and `toJson` for every entity, defaults, unknown fields kept, bad files reported |
| `tst_layout_tree` | every edit operation, each normalisation rule on its own and combined, size scaling when splits merge, `sameShape` |
| `tst_store_load` | loading the fixtures, the load problem list |
| `tst_write_back` | `serialise(load(fixtures))` equals the fixture files, compared as parsed JSON. This covers the `timetrack` data of the missing module. |
| `tst_sorting` | priority, order, Default first, filters |
| `tst_fuzzy` | ranking and edge cases |

`tests/items/tst_registry` covers id checks, the order of `types()`, and `UnknownItemType` for unknown ids.

**Done when** all these tests pass in CI.

## Part 4: layout widgets

**`LayoutView`** shows one tree. It takes the tree and a `CellFactory` that makes cell widgets for references. A task dashboard and a root dashboard use different factories.

- It builds `QSplitter` for splits, `QTabWidget` for tabs and `CellFrame` for cells.
- **Updating.** When the model sends a new tree:
  - if `sameShape(old, new)` is true, only the splitter sizes and current tabs are set, in place;
  - otherwise the splitters and tab widgets are built again. Existing `CellFrame`s are kept in a pool keyed by their reference and occurrence, taken out of the old widgets, and placed in the new ones. A frame whose item type has `reparentSafe` off is destroyed and made again. Frames left in the pool are destroyed.
- **Sizes.** Ratios become `QSplitter` sizes by scaling them to the splitter's length. When the user drags a splitter, a 300 ms timer after the last `splitterMoved` writes the new ratios to the model. Because only sizes change, the view is not rebuilt during the drag.
- **Current tab.** Changing the tab writes `current` to the model in the same way.
- **Single view.** For an item type with `singleView` on, the view asks a shared `SingleViewTracker` whether another placement holds the item. If one does, the cell shows "Shown in another cell" with a "Show here" button. No M2 type sets the flag. A test type in `tests/` does, for manual checking.

**`CellFrame`** has a header and a body.

- The header shows the type icon and the item name. In edit mode it is also a drag handle. The buttons for refresh, open in browser and detach arrive with their milestones. The menu button opens the cell menu.
- The body is a placeholder: the type name, the item name and the item id. For an `UnknownItemType` it says: "This item needs the `<module>` module, which this build does not include."
- Empty cells show "Empty". Cells with `removed` set show "Removed".
- A focused cell gets a highlighted border.

**`TaskCell`** is the cell of a task in a root dashboard. It has a thin header with the task name, a dashboard picker when the task has more than one dashboard, the task dashboard's lock button and an "open alone" button. Its body is a nested `LayoutView` on the chosen task dashboard. Edits there change the task dashboard, as in [decision 0011](../decisions/0011-root-cells-reference-task-dashboards.md). The chosen dashboard is stored in the cell's reference.

**Lock state.** Each dashboard starts locked. A set of unlocked dashboard ids is kept in memory by `MainWindow` and passed to the views. Resizing splitters and switching tabs work while locked.

**`LayoutEditor`** turns user actions into `LayoutTree` operations. It then calls the `Model` method, which normalises.

- **Cell menu:** split right, split down, wrap in tabs, add tab, change item, clear cell, remove cell. "Change item" lists the task's items, or for a root dashboard, the tasks and the items of the task in the cell.
- **Tab menu:** rename, close, move left, move right, collapse when single.

**Drag and drop.**

- **MIME type** `application/x-mirante-ref`: a JSON object with the reference, plus the source dashboard and path when a cell is moved.
- **Sources:** cell headers in edit mode, and sidebar tasks and items, which can be dropped even on a locked dashboard, as [ui](../ui.md#placing-tasks-and-items) says.
- **`DropOverlay`** is a transparent child of the top `LayoutView`, shown while a drag is over it. It finds the cell under the cursor and divides it into five zones: each edge is the outer 25%, and the rest is the centre. Over a tab bar, it shows an insert marker instead. It returns a zone, and `LayoutEditor` performs the move or insert.
- **Rules from [ui](../ui.md#editing-layouts):**
  - a task dashboard accepts only items of its own task;
  - the centre of an empty cell fills it;
  - a task dropped onto a cell inside a task cell lands beside that task cell.

**Done when** every editing action in [ui](../ui.md#editing-layouts) works by hand on the fixtures on Windows, and CI is green.

## Part 5: main window

**`MainWindow`** is a horizontal `QSplitter`: the sidebar on the left, and the content on the right. The content holds the `RootTabBar` over a `QStackedWidget` of dashboards, and a warning bar for load problems.

**`Sidebar`.**

- A `QTreeView` on a `SidebarModel`, a `QAbstractItemModel` over `Model`. The tree shows groups, then projects, then tasks, then items.
- Groups use manual order. Projects and tasks are sorted with `Sorting`.
- A delegate paints the priority marker and dims Low entries.
- Two filter buttons hide Low entries and done or archived tasks. The second is on by default. A button collapses the sidebar.
- **Clicks:** a single click on a task opens the preview, and a double click pins it.
- **Context menu in M2:** "Open in new root dashboard" only. The rest arrives with editing in M3.
- **Not in M2:** the Focus entry and sidebar reordering arrive in M3.

**`RootTabBar`** is a `QTabBar` subclass.

- The preview tab is drawn with an italic title by overriding `paintEvent` for that one tab, and has a pin button.
- Pinning makes a root dashboard with one task cell, in memory, and puts its tab first.
- Clicking a task that already has a pinned tab switches to it.
- The `+` button makes an empty root dashboard with editing on.
- Closing a tab hides it. The quick switcher opens it again.

**`QuickSwitcher`.** `Ctrl+K` opens a popup with a line edit and a list, ranked by `FuzzyMatch` over groups, projects, tasks, items and root dashboards. `Enter` acts on the chosen entry:

- a task opens its preview;
- an item opens its task's preview and focuses its cell;
- a root dashboard opens its tab;
- a group or project is selected in the sidebar.

**`Shortcuts`.** Each shortcut is a `QShortcut` on the main window with `Qt::WindowShortcut`. `QKeySequence` already maps `Ctrl` to `Cmd` on macOS.

- **`Ctrl+Alt+Arrow`** moves focus to the nearest visible cell in that direction, measured from the centre of the focused cell. Cells inside other tabs are not visible and are skipped.
- **`Ctrl+Tab`** and **`Ctrl+Shift+Tab`** change the tab in the tab group that holds the focused cell.
- **`Ctrl+PgDown`** and **`Ctrl+PgUp`** change the root tab.
- **`Ctrl+Shift+M`** maximises the focused cell by moving its frame into a layer that fills the content area, and moves it back on the second press. This uses the same reparenting as layout updates. A frame whose type is not `reparentSafe` is made again.
- **`Ctrl+W`** closes the preview tab or the focused root tab.

The focused cell follows `QApplication::focusChanged`.

**Done when** every M2 item in the roadmap is checked by hand on Windows, CI is green, and the roadmap boxes are ticked.

## Order of work

Parts 1 and 3 change separate folders. So do parts 4 and 5.

| Step | Work | Worktree branch | Can run beside |
|---|---|---|---|
| 1 | Part 1 | `m2-build` | none, everything builds on it |
| 2 | Part 2 | `m2-modules` | step 3 |
| 3 | Part 3 | `m2-core` | step 2 |
| 4 | Part 4 | `m2-layout` | step 5, once step 3 is merged |
| 5 | Part 5 | `m2-ui` | step 4, once step 3 is merged |

Step 5 needs `LayoutView` to show dashboards. Until step 4 is merged, it uses a plain label in the content area. Each step is merged into `master` when its build and tests pass, locally and in CI.

## Checking

- **Automatic:** the tests above, run by CTest locally and in CI on three platforms, with all modules and with none.
- **By hand on Windows:** every acceptance criterion of M2, with the fixtures and with `--data` on a copy of them.
- **macOS and Linux:** by CI only. Their builds are not checked by hand.

## Out of scope

- Writing files, reloading and conflict copies: M3.
- Creating, editing and deleting entities other than root dashboards in memory: M3.
- The Focus view and device settings: M3.
- Detached windows, item views and web content: M4.
- `ItemContext`, `createView` and every module header beyond `Module.h` and `ItemType.h`: they are added in the milestone that first needs them.
