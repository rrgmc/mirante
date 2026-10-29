# M3 code plan: data folder and editing

This plan implements [milestone M3](../roadmap.md#m3-data-folder-and-editing). After M3 the app is usable day to day: everything can be created, edited and deleted, every change is saved to the workspace, and two devices can share the workspace through a sync tool.

M3 still has no web views and no API calls. Item cells keep their M2 placeholders.

## Changes to the architecture

- **Item types declare their fields, or bring an editor, now rather than in M5.** The item dialog is built from the fields of the chosen type, or uses the type's own editor, so `Fields.h` and `ItemEditor.h` join the module interface in M3. See [decision 0028](../decisions/0028-item-fields-in-m3.md).
- **The Store becomes an object.** M2's `store::load()` and `store::serialise()` stay as free functions. A `Store` class uses them, and adds writing, watching and conflict copies. It is built from two smaller classes, `FileWriter` and `FolderWatcher`, so they can be written and tested apart.
- **Edits to a task are pure functions.** Code that changes a task, such as removing an item and clearing its cells, is a function from task to task in `core/Edits`, like the layout operations. The `Model` stores the result. This keeps the rules testable without a model.
- **`MainWindow.cpp` does not grow.** New behaviour goes into new classes: `EntityActions`, `FocusView`, `ConflictBar`, `ImagePaste` and the dialogs. Only the last task connects them in `MainWindow`.

[architecture.md](../architecture.md) gets the new classes in the commit of the task that adds each one.

## Part 1: Model edits

**Signals.** The model gains `groupAdded`, `groupChanged`, `groupRemoved`, the same three for projects, and `taskAdded` and `taskRemoved`. `taskChanged` and the root dashboard signals stay. Each carries the entity id.

**Changes.** Each entity has one setter that adds the entity or replaces it, and one remover:

```cpp
void setGroup(const Group &group);           // adds, or replaces the group with that id
void setProject(const Project &project);
void setTask(const Task &task);              // normalises its dashboard layouts
void setRootDashboard(const RootDashboard &dashboard);
void removeGroup(const QString &id);         // removes its projects and their tasks first
void removeProject(const QString &id);       // removes its tasks first
void removeTask(const QString &id);
// removeRootDashboard() exists since M2.
```

- Every UI edit copies an entity, changes the copy, and calls the setter. The Store's reload calls the same setters. One path keeps the signals the same for local and external changes, as [architecture](../architecture.md#data-flow) asks.
- A setter that receives a value equal to the stored one emits nothing. Entities gain `operator==`, comparing every field including `extra`.
- Removing a task does not rewrite root dashboards that show it. Their cells show "Removed" through `refExists`, and the file is cleaned up the next time it changes for another reason. This follows [sync](../sync.md#writing).
- `setProject` with a new `groupId` moves the project; `setTask` with a new `projectId` moves the task.
- `reset()` stays for loading a whole workspace.

**`core/Edits.h`**, pure functions:

```cpp
namespace mirante::edits {

QString newId();                                   // a UUID without braces

Task newTask(const QString &projectId, const QString &title);   // one dashboard, one empty cell
Task addItem(Task task, const Item &item);
Task removeItem(Task task, const QString &itemId);  // its cells become empty cells
Task addDashboard(Task task, const Dashboard &dashboard);
Task removeDashboard(Task task, const QString &dashboardId);    // refuses the last; moves the default
Task setDefaultDashboard(Task task, const QString &dashboardId);

// Moves a project or task to index within the list of its siblings at the
// given priority, and renumbers order in that list from 0. Returns every
// sibling whose priority or order changed, ready for the setters.
std::vector<Project> moveProject(std::vector<Project> siblings, const QString &id, Priority priority, int index);
std::vector<Task> moveTask(std::vector<Task> siblings, const QString &id, Priority priority, int index);
std::vector<Group> moveGroup(std::vector<Group> groups, const QString &id, int index);

// Every string anywhere in the items' config values. Used to tell whether a
// file in files/ is still referred to, for any module, present or not.
QSet<QString> referencedStrings(const std::vector<const Task *> &tasks);

}
```

**Tests:** `tst_edits` covers every function. `tst_model_edits` covers the setters, the cascades, the signals emitted, and that an equal value emits nothing.

## Part 2: migrations and conflict copy names

**`core/Migrations.h`.**

```cpp
namespace mirante::migrations {

enum class FileKind { Workspace, Group, Project, Task, RootDashboard };
constexpr int currentSchemaVersion = 1;

// Upgrades json in place to currentSchemaVersion. Returns false and sets
// *error for a version newer than this app's, or a missing version.
bool migrate(FileKind kind, QJsonObject &json, QString *error = nullptr);

// The same with a given list of steps, for tests. Step n upgrades n to n + 1.
using Step = std::function<void(FileKind, QJsonObject &)>;
bool migrate(FileKind kind, QJsonObject &json, const std::vector<Step> &steps, QString *error = nullptr);

}
```

Version 1 is the only version, so the real list of steps is empty. A file with a newer version is skipped and reported as "written by a newer version of Mirante", as M2 does today, so this app never writes over data it does not understand. A migrated file is written back in the new version only when it changes for another reason, as [sync](../sync.md#the-data-folder) says.

**`core/ConflictNames.h`.**

```cpp
namespace mirante::conflicts {
// For a file name in the data folder, the name of the file it is a conflict
// copy of, or an empty string if it is not a conflict copy.
QString originalOf(const QString &fileName);
}
```

Entity files are named after a UUID. Any `.json` file whose name starts with a UUID but is not exactly `<uuid>.json` is a conflict copy of `<uuid>.json`. This one rule covers Syncthing, OneDrive and Dropbox without knowing their formats. The same holds for `workspace*.mirante` and for images in `files/`. See [decision 0031](../decisions/0031-conflict-copies-by-id-prefix.md).

**Tests:** `tst_migrations` with fake steps, newer and missing versions. `tst_conflict_names` with real examples from Syncthing, OneDrive and Dropbox, and with names that are not copies.

## Part 3: writing and watching

**`core/FileWriter.h`** writes files in the data folder.

```cpp
class FileWriter : public QObject {
public:
    explicit FileWriter(const QString &dir, QObject *parent = nullptr);
    void setDelay(int ms);                         // 500 by default; tests use 0
    // Schedules a write; content nullopt deletes the file. A later call for the
    // same path replaces an earlier one that has not been written yet.
    void schedule(const QString &relPath, std::optional<QByteArray> content);
    void flush();                                  // writes everything pending now
    bool pending() const;
    // Whether the file on disk holds exactly what this writer last wrote there.
    bool isOwnWrite(const QString &relPath, const QByteArray &content) const;
signals:
    void writeFailed(const QString &relPath, const QString &message);
    void written(const QString &relPath);
};
```

- It writes with `QSaveFile`, and creates missing subfolders.
- It remembers a SHA-1 hash of each file it wrote or deleted, for `isOwnWrite`.
- A failed write stays pending and is tried again with the next write, and reported once per path until it succeeds.

**`core/FolderWatcher.h`** reports changes made by others.

```cpp
class FolderWatcher : public QObject {
public:
    FolderWatcher(const QString &dir, std::function<bool(const QString &, const QByteArray &)> isOwnWrite,
                  QObject *parent = nullptr);
    void setDelay(int ms);                         // 300 by default
    void start();                                  // records the files present now
signals:
    void filesChanged(const QStringList &relPaths);   // added or modified, not own writes
    void filesRemoved(const QStringList &relPaths);
    void conflictCopiesChanged(const QStringList &relPaths);  // every copy present now
};
```

- It watches the top folder and its five subfolders with `QFileSystemWatcher`. Directory events start a scan after 300 ms of quiet. The scan compares each file's size and modification time with the last scan, and reads only the files that differ.
- It skips own writes through `isOwnWrite`, and files whose name `conflicts::originalOf` recognises, which it reports apart.
- It ignores the temporary files of `QSaveFile` and of sync tools, such as `.syncthing.*.tmp` and `~*`.
- A subfolder that appears after `start()` is added to the watch.

**`Store`** in `core/Store.h` ties them to the model.

```cpp
class Store : public QObject {
public:
    explicit Store(Model &model, QObject *parent = nullptr);
    // Loads dir into the model, then watches it and writes changes. A resource
    // path such as ":/fixtures" opens read-only.
    std::vector<LoadProblem> open(const QString &dir);
    void close();                                  // flushes and stops watching
    QString dir() const;
    bool readOnly() const;
    void flush();

    // files/: saves data under a new name such as "<uuid>.png" and returns the name.
    QString saveFile(const QByteArray &data, const QString &extension, QString *error = nullptr);
    QString filePath(const QString &name) const;   // an absolute path, or a resource path

    struct Conflict { QString file; QString copy; };
    std::vector<Conflict> conflicts() const;
    void keepCurrent(const Conflict &conflict);    // deletes the copy
    void useCopy(const Conflict &conflict);        // the copy replaces the file, and is reloaded
signals:
    void problemsChanged(const std::vector<LoadProblem> &problems);  // load, reload and write problems
    void conflictsChanged();
};
```

- **Writing.** The Store listens to the model's signals. Each signal names one file: a group, project or task file, a root dashboard file, or `workspace.mirante`. The Store schedules that file with `FileWriter`, with content from a new function `store::fileFor(model, relPath)`, which returns `nullopt` when the entity is gone. `serialise()` is rewritten on top of `fileFor()`, so the write-back test keeps covering it.
- **Reloading.** For changed files, the Store parses and migrates them and calls the model's setters, or reports a load problem. For removed files it calls the removers. While it applies a reload it ignores the model's signals, so a reload is never written back.
- **Read-only.** The sample data is read-only: edits change the model only, and the warning bar says that changes to the sample data are not saved. See [decision 0029](../decisions/0029-sample-data-read-only.md).
- **Image files.** When an item edit on this device removes the last reference to a file in `files/`, the Store deletes the file. It never deletes a file it finds unreferenced on disk, because the task that refers to it may not have synced yet. See [decision 0030](../decisions/0030-deleting-image-files.md).
- **Conflict copies.** `conflicts()` pairs each copy with its file. `useCopy` writes the copy's content over the file, deletes the copy and reloads. Showing both side by side is the UI's job.
- `store::saveWorkspaceName()` stays for renaming, but `MainWindow` now renames through `Model::setWorkspaceName()` and the Store writes the file.

**Tests:**

| Test | Covers |
|---|---|
| `tst_file_writer` | debouncing, replacement of a pending write, deletion, `isOwnWrite`, a failed write retried |
| `tst_folder_watcher` | an external change is reported once after the delay, own writes are skipped, removed files, conflict copies, a new subfolder |
| `tst_store_write` | each kind of edit writes the right file with sorted keys and `schemaVersion`; removals delete files; nothing is written for the sample data; image files deleted only after the last reference goes |
| `tst_store_reload` | an external change updates the model in place; a broken file is reported and the model keeps the old entity; a removed file removes the entity; a reload is not written back |
| `tst_store_conflicts` | detection, `keepCurrent`, `useCopy` |
| `tst_write_back` | unchanged, on the new `serialise()` |

Two app instances on one folder are checked by hand, and by `tst_store_reload` with two `Store`s on one temporary folder.

## Part 4: device settings

**`core/DeviceSettings.h`** gives typed access to the values in [sync](../sync.md#device-settings) that M3 uses. Every other `QSettings` call in `ui/` and `main.cpp` moves here.

```cpp
class DeviceSettings {
public:
    explicit DeviceSettings(QSettings *settings = nullptr);   // tests pass their own
    QString lastWorkspace() const;               void setLastWorkspace(const QString &);
    QStringList recentWorkspaces() const;        void addRecentWorkspace(const QString &);
    void removeRecentWorkspace(const QString &); void clearRecentWorkspaces();
    QString folderPath(const QString &folderId) const;
    void setFolderPath(const QString &folderId, const QString &path);   // empty removes it
    // The open tabs of one workspace, in order, and the current one.
    struct Tabs { QStringList ids; QString current; QString previewTask, previewDashboard; };
    Tabs openTabs(const QString &workspace) const; void setOpenTabs(const QString &workspace, const Tabs &);
    // Window size, maximized and splitter state, as M2 stores them now.
    QSize windowSize() const; ...
};
```

Folder ids are UUIDs, so one map serves every workspace. Open tabs are kept per workspace, keyed by the workspace folder's path. At start the app opens the tabs that were open when it last closed that workspace. The first time a workspace is opened, every root dashboard is opened, as in M2.

**Tests:** `tst_device_settings` on a `QSettings` in a temporary INI file.

## Part 5: fields and dialogs

**`mirante/Fields.h`**, in the module interface:

```cpp
namespace mirante {

struct Field {
    enum class Type { Text, MultiLineText, Url, Integer, Bool, Choice, Folder, Image };
    QString key;             // the key in Item::config
    QString label;
    Type type = Type::Text;
    QJsonValue defaultValue;
    bool required = false;
    QStringList choices;     // Choice: the stored values
    QStringList choiceLabels;// Choice: the labels shown, same length
    QString placeholder;
};

}
```

**`mirante/ItemEditor.h`**, in the module interface, for types that bring their own editor:

```cpp
namespace mirante {

// What a form or an editor can reach while an item is edited.
struct EditorContext {
    std::vector<std::pair<QString, QString>> folders;   // the project's folders: id, name
    std::function<QString(const QImage &)> saveImage;   // saves into files/, returns the name or empty
};

class ItemEditor : public QWidget {
public:
    using QWidget::QWidget;
    virtual void setConfig(const QJsonObject &config) = 0;
    virtual QJsonObject config() const = 0;     // keeps keys it does not show
    virtual QStringList problems() const { return {}; }   // empty when valid
};

}
```

`ItemType` gains three methods:

```cpp
virtual std::vector<Field> fields() const { return {}; }
// An editor of the type's own, or null to use a form built from fields().
virtual ItemEditor *createEditor(const EditorContext &context, QWidget *parent) const { return nullptr; }
// The key of an Image field that a pasted image fills. If set, pasting an image
// onto a task dashboard creates an item of this type. See docs/item-types.md#image.
virtual QString pastedImageField() const { return {}; }
```

- Each module chooses per type: typed fields, or its own editor. See [decision 0028](../decisions/0028-item-fields-in-m3.md).
- A `Folder` field stores the id of one of the project's folders. An `Image` field stores a file name in `files/`.
- `UnknownItemType` has neither. The item dialog then shows only the name, and keeps `config` unchanged.
- Each module declares the fields listed for its types in [item types](../item-types.md). The M3 types all fit typed fields, so no module needs its own editor yet; a test type in `tests/` has one, so the dialog's editor path is covered. The image module returns `"file"` from `pastedImageField()`.

**`items/FieldValidation.h`:** `validate(fields, config)` returns a message for each invalid field: a required field that is empty, an integer that is not one, a URL without `http` or `https`, a choice not in the list. Keys not in `fields` are kept and not checked.

**`ui/FieldForm.h`:** an `ItemEditor` built from a list of fields and an `EditorContext`. It reads back the edited config, keeping keys it does not show. Folder fields list the project's folders. Image fields show a thumbnail, "Paste" and "Choose file…"; they save through `EditorContext::saveImage`, which wraps `Store::saveFile`.

**Dialogs** in `ui/`. Each takes an entity, lets the user edit it, and returns the edited copy. Each refuses an empty name.

| Dialog | Edits |
|---|---|
| `GroupDialog` | name |
| `ProjectDialog` | name, group, priority; folders: name, remote URL, main, and the local path on this device with "Choose…", shown as "Folder not set on this device" when empty. Exactly one folder is main. |
| `TaskDialog` | title, project, priority, state |
| `ItemDialog` | type (only when creating), name, and the type's own editor or else a `FieldForm`; OK is refused while `problems()` is not empty |
| `DashboardDialog` | name, and when creating a [preset](../ui.md#layout-presets) |

Module settings and connections in the project and group dialogs arrive in M5.

**`layout/Presets.h`:** `presetNames()` and `buildPreset(index, itemIds)`. The Tabs preset puts one item in each tab; the others fill cells with the items in order and leave the rest empty.

**Tests:** `tst_field_validation`; `tst_presets`; `tst_field_form`, which builds a form for every field type and reads back an unchanged config.

## Part 6: sidebar, actions and Focus

**`ui/EntityActions.h`** holds every create, edit and delete command, so the sidebar, the menus and the switcher share them.

```cpp
class EntityActions : public QObject {
public:
    EntityActions(Model &model, Store &store, const ItemTypeRegistry &types,
                  DeviceSettings &settings, QWidget *dialogParent);
    void newGroup(); void newProject(const QString &groupId); void newTask(const QString &projectId);
    void newItem(const QString &taskId); void newDashboard(const QString &taskId);
    void edit(const QString &id);                 // any entity, by its id
    void remove(const QString &id);               // asks for confirmation first
    void setPriority(const QString &id, Priority priority);
    void setState(const QString &taskId, TaskState state);
    void setDefaultDashboard(const QString &taskId, const QString &dashboardId);
    QMenu *contextMenu(const QString &id, QWidget *parent);   // the sidebar menu for that row
};
```

- The sidebar context menu holds new, edit, rename, set priority, set state, delete, and on dashboards "Set as default" and on items and dashboards "Open in new root dashboard", as [ui](../ui.md#sidebar-tree) lists.
- Delete confirmations name what goes with the entity, such as "Delete project backend and its 4 tasks?".
- A new task is created with `edits::newTask()`, so it has one dashboard with one empty cell.
- The last dashboard of a task cannot be deleted; the command is disabled.

**Sidebar drag and drop.** `SidebarModel` accepts drops of projects and tasks among their siblings, and of groups among groups.

- Dropping between two rows of the same priority changes the order.
- The dropped row takes the priority of the row above the drop position, or of the row below when dropped first in the list. This is how a drag across a boundary between two levels changes the priority.
- Dragging a project to another group, or a task to another project, is not supported. The dialogs move them.
- Every change goes through `edits::moveProject`, `moveTask` or `moveGroup`, then the setters.

**Focus.** The sidebar gains the Focus entry at the top, above the tree. It opens `FocusView` in a "Focus" tab, at the start of the root tab bar, at most one at a time.

- `FocusView` lists active tasks of every group, or of one group chosen in a combo box. Each row shows the task, its project and group, and the priority marker.
- The order comes from a new `Sorting` function, `focusOrder(tasks, model)`: task priority, then project priority, then the task's order.
- A double click or `Enter` opens the task's default dashboard in the preview tab.
- The "Needs attention first" toggle arrives with attention in M5.

[ui.md](../ui.md#focus-view) says where Focus opens, in the commit that adds it.

**Tests:** `tst_sorting` gains `focusOrder`. `tst_sidebar_model` gains drops, checked with `QAbstractItemModelTester`. `tst_focus_view` checks the rows and the group filter on the fixtures.

## Part 7: images, conflicts and the main window

**`ui/ImagePaste.h`.** `Ctrl+V` with an image on the clipboard, while a task dashboard or an item tab has the focus:

- If the focused cell holds an item whose type has a `pastedImageField`, the image replaces that item's image. This is an item edit, so it works while the dashboard is locked.
- Otherwise, on an unlocked task dashboard, a new item of the first type with a `pastedImageField` is created, named "Image" and numbered if needed. An empty focused cell holds it; any other focused cell is split to the right.
- On a locked dashboard, or a root dashboard, it shows a short message in the status bar. A root dashboard cell must belong to a task, so the user pastes into the task dashboard.
- Without a type that has a `pastedImageField`, as in a build without the `image` module, paste does nothing.

The image is saved as PNG through `Store::saveFile`.

**`ui/ConflictBar.h`.** A bar under the warning bar, one line per conflict copy: "Two versions of task Fix login timeout", with "Keep current", "Use other" and "Compare". Compare opens a dialog with both files side by side, as formatted JSON, with the differing lines marked, and the same two choices.

**`MainWindow` and `main.cpp`.**

- `main.cpp` creates `DeviceSettings` and `Store` in the order of [architecture](../architecture.md#ownership-and-lifetime), and flushes the Store before quitting.
- Opening a workspace goes through `Store::open`. The warning bar shows the Store's problems, and the read-only note for the sample data.
- File gets New Group…; the sidebar header gets a `+` menu with New Group, New Project and New Task.
- The context menus come from `EntityActions`. `ImagePaste`, `ConflictBar` and the Focus tab are placed.
- Open tabs are saved per workspace and restored.
- Renaming the workspace goes through the model.

**Tests:** `tst_main_window` gains: an edit through `EntityActions` reaches the file on disk; open tabs are restored after reopening a workspace.

## Tasks for parallel work

The rules, the roles and the brief are those of the [M2 plan](m2-app-skeleton.md#tasks-for-parallel-work), with `m2-app-skeleton.md` replaced by this plan. At most seven agents run at the same time.

| Task | Branch | Work | Owns | Needs |
|---|---|---|---|---|
| T0 | `m3-interfaces` | Every header named in this plan, with stubs that compile. The new `Model` signals and methods, declared. `Fields.h`, `ItemEditor.h`, and the three new `ItemType` methods. | everything it creates; after merging, the headers belong to the orchestrator | none |
| T1 | `m3-model-edits` | Part 1: the model setters, removers and signals, entity `operator==`, `core/Edits`. `tst_edits`, `tst_model_edits`. | `src/core/Model.cpp`, `Entities.cpp`, `Edits.cpp`, their tests | T0 |
| T2 | `m3-migrations` | Part 2: `Migrations` and `ConflictNames`. Their tests. T11 makes the load path call `migrate()`. Updates [sync](../sync.md#conflict-copies). | `src/core/Migrations.cpp`, `ConflictNames.cpp`, their tests | T0 |
| T3 | `m3-file-writer` | `FileWriter`. `tst_file_writer`. | `src/core/FileWriter.cpp`, its test | T0 |
| T4 | `m3-device-settings` | Part 4: `DeviceSettings`. `tst_device_settings`. | `src/core/DeviceSettings.cpp`, its test | T0 |
| T5 | `m3-fields` | `FieldValidation`, `FieldForm`, `Presets`. Their tests. Updates [modules](../modules.md#the-module-interface) and [item types](../item-types.md#the-type-registry). | `src/items/FieldValidation.cpp`, `src/ui/FieldForm.cpp`, `src/layout/Presets.cpp`, their tests | T0 |
| T6 | `m3-focus` | `focusOrder` and `FocusView`. `tst_focus_view`. Updates [ui](../ui.md#focus-view). | `src/core/Sorting.cpp`, `src/ui/FocusView.cpp`, their tests | T0 |
| T7 | `m3-folder-watcher` | `FolderWatcher`. `tst_folder_watcher`. | `src/core/FolderWatcher.cpp`, its test | T2, T3 |
| T8 | `m3-module-fields` | `fields()` for every item type of the seven modules, and `pastedImageField()` for `image`. No module needs its own editor in M3. Each module's `tst_register` checks that its fields validate their defaults. | `src/modules/` | T5 |
| T9 | `m3-dialogs` | The five dialogs of Part 5, and the test type with its own editor. | `src/ui/*Dialog.cpp` except `RootDashboardsDialog.cpp` | T4, T5 |
| T10 | `m3-sidebar-dnd` | Sidebar drops and the Focus entry. `tst_sidebar_model` additions. | `src/ui/Sidebar*.cpp`, `tests/ui/tst_sidebar_model.cpp` | T1 |
| T11 | `m3-store` | The `Store` class and `store::fileFor`, on `FileWriter` and `FolderWatcher`. `tst_store_write`, `tst_store_reload`, `tst_store_conflicts`. Updates [sync](../sync.md) and [architecture](../architecture.md#core). | `src/core/Store.cpp`, its tests | T1, T2, T3, T7 |
| T12 | `m3-entity-actions` | `EntityActions` and its context menus. | `src/ui/EntityActions.cpp` | T1, T9 |
| T13 | `m3-image-paste` | `ImagePaste`. Updates [item types](../item-types.md#image). | `src/ui/ImagePaste.cpp` | T8, T11 |
| T14 | `m3-conflict-bar` | `ConflictBar` and its compare dialog. | `src/ui/ConflictBar.cpp` | T11 |
| T15 | `m3-main-window` | Part 7's `MainWindow` and `main.cpp` changes. `tst_main_window` additions. | `src/ui/MainWindow.cpp`, `src/main.cpp`, `tests/ui/tst_main_window.cpp` | all of T1 to T14 |
| T16 | none | Done by the orchestrator: write `docs/plans/m3-checks.md` for the checks by hand, run both presets, and tick the roadmap once the user has checked. | any | all |

### Waves

| Wave | Tasks | At the same time |
|---|---|---|
| 0 | T0, then the user reviews the headers | 1 |
| 1 | T1, T2, T3, T4, T5, T6 | 6 |
| 2 | T7, T8, T9, T10 | 4 |
| 3 | T11, T12 | 2 |
| 4 | T13, T14 | 2 |
| 5 | T15 | 1 |
| 6 | T16 | 1 |

## Checking

- **Automatic:** the tests above, by CTest locally and in CI on three platforms, with all modules and with none.
- **By hand on Windows:** the M3 acceptance criteria, listed in `docs/plans/m3-checks.md` by T16. Two instances are checked on one folder with two `--data` arguments, and a conflict copy is made by hand by copying a task file to `<uuid>.sync-conflict-test.json`.

## Out of scope

- "Add to dashboard…" in the sidebar menu, which [ui](../ui.md#placing-tasks-and-items) describes. Dragging covers it. It is not on any milestone yet.
- The "done" suggestion for tasks, which needs live data: M6.
- Module settings and connections in the dialogs: M5.
- Pasting a URL to pick the item type: M6.
- Suggesting a folder's local path from its remote URL: M7, with `GitHead`.
- Detached window geometry and the other device settings of later milestones.
