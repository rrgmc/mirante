# M7 code plan: quick add, Claude local, folders and notes

This plan implements [milestone M7](../roadmap.md#m7-quick-add-folders-and-notes). After M7, a pasted Jira key or merge request link makes a whole task, a local Claude session resumes in a terminal, the Folder item shows its Git state and opens tools, the CI Pipeline can follow the main folder's branch, and the Notes and Image items have real views.

## Decisions

All accepted:

- [Decision 0078](../decisions/0078-quick-add-dialog.md): quick add is a dialog that runs the modules' handlers and finders asynchronously, in a profile the user can change, and creates a task with the chosen items in one cell of its dashboard.
- [Decision 0079](../decisions/0079-module-device-settings.md): modules declare device settings as fields. The settings window edits them, and they are kept in `QSettings`, never synced.
- [Decision 0080](../decisions/0080-git-read-by-the-core.md): the core reads the branch from `.git/HEAD` itself, and runs `git status` for the summary. Modules ask through `ItemContext` and `StatusJob`.
- [Decision 0081](../decisions/0081-views-that-keep-their-own-keys.md): an item type can name configuration keys that its view writes itself. A change to only those keys does not make the view again; the view is told instead. Notes need this, because they save while the user types.
- [Decision 0082](../decisions/0082-commands-as-argument-lists.md): commands for terminals and editors are passed as a program and a list of arguments, never as one shell string. The core quotes them for each platform.

Smaller choices made here:

- **Claude session ids are checked.** An id holds letters, digits and dashes only. Anything else shows a message and starts nothing, so a synced item cannot run a command on another device.
- **Notes are stored in the item's configuration,** under `text`. The item dialog shows no field for it, and keeps it because it keeps keys its form does not show. Notes already sync this way, as [data model](../data-model.md#what-is-synced) says.
- **A note that changes elsewhere while the user types:** the local text wins when it is saved. A note is written by one person, and two devices editing the same note at the same moment is rare. The other device's text is lost, as with any last write to an item.
- **The folder view refreshes itself** when it shows and every 10 seconds while visible. It has no status, so the status cache does not poll it.
- **No device settings for items.** [Decision 0066](../decisions/0066-fixed-native-layouts-with-sections.md) waits for them to remember collapsed sections and column widths. They are not in M7's boxes, and module device settings do not need them. See [out of scope](#out-of-scope).

## Changes to the architecture

- **`QuickAdd.h` in the module interface,** as [modules](../modules.md#quick-add) planned. A handler reads pasted text and proposes a task. A finder offers more items for a task.
- **New `ModuleRegistrar` members:** `addQuickAddHandler()`, `addRelatedItemFinder()` and `addDeviceSetting()`.
- **New `ItemContext` members:** `deviceSetting()`, `deviceName()`, `gitHead()`, `gitStatus()`, `launchTerminal()`, `startProgram()`, `showFolder()`, `mapFolder()`, `filePath()` and `onConfigChanged()`. All have defaults, so existing fake contexts need no change. `folderPath()` with an empty id gives the main folder.
- **New `StatusJob` members:** `folderPath()` and `gitHead()`. The status cache watches each `.git/HEAD` that a job read, and fetches the item again when it changes.
- **New `ItemType` member:** `viewKeys()`, the keys of decision 0081.
- **New core classes:** `GitHead` and `GitStatusReader` in `core/`, `Launcher` in `platform/`, `QuickAddRun` in `items/`, and `QuickAddDialog` in `ui/`.
- **`DeviceSettings`** gains the terminal command, the device name and the module device settings.

## Part 1: the module interface

```cpp
// QuickAdd.h
struct QuickAddItem
{
    QString typeId;
    QString name;
    QJsonObject config;
};

// A project setting to match, such as gitlab.project = "acme/backend".
struct ProjectMatch
{
    QString key;          // a project setting of the handler's module
    QJsonValue value;
};

struct QuickAddProposal
{
    QString title;                    // the task's title
    std::vector<QuickAddItem> items;
    std::optional<ProjectMatch> project;
    QStringList texts;                // for finders: the title, a branch, a key
};

// One run of a handler. Like StatusJob: the module's own settings and server
// in the dialog's profile, and finish() exactly once.
class QuickAddRequest
{
public:
    virtual ~QuickAddRequest() = default;
    virtual QString text() const = 0;     // trimmed pasted text
    virtual QJsonObject moduleSettings(const QString &screenId) const = 0;
    virtual ApiClient *server() = 0;      // null for a module without a ServerType
    virtual void finish(std::optional<QuickAddProposal> proposal) = 0;
};

class QuickAddHandler
{
public:
    virtual ~QuickAddHandler() = default;
    // Calls request.finish(), with nothing if the text is not for this handler.
    virtual void propose(QuickAddRequest &request) const = 0;
};

// One run of a finder, after the project is chosen.
class RelatedItemRequest
{
public:
    virtual ~RelatedItemRequest() = default;
    virtual QStringList texts() const = 0;     // the proposal's texts and the task title
    virtual std::vector<QuickAddItem> items() const = 0;   // already offered, to skip
    virtual QJsonValue projectSetting(const QString &key) const = 0;
    virtual QJsonObject moduleSettings(const QString &screenId) const = 0;
    virtual ApiClient *server() = 0;
    virtual void finish(std::vector<QuickAddItem> items) = 0;
};

class RelatedItemFinder
{
public:
    virtual ~RelatedItemFinder() = default;
    virtual void find(RelatedItemRequest &request) const = 0;
};

// Module.h
virtual void addQuickAddHandler(std::unique_ptr<QuickAddHandler> handler) = 0;
virtual void addRelatedItemFinder(std::unique_ptr<RelatedItemFinder> finder) = 0;
// A field kept on this device. Types Text, Integer, Boolean and Choice only.
virtual void addDeviceSetting(const Field &field) = 0;

// ItemType.h
// Keys the view writes itself with updateConfig(). A change to only these
// keys does not make the view again; onConfigChanged() reports it. Decision 0081.
virtual QStringList viewKeys() const { return {}; }

// ItemContext.h
struct GitState
{
    enum class Result { Ok, NotARepository, GitMissing, Failed };
    Result result = Result::Failed;
    QString branch;            // empty when detached
    QString commit;            // short hash
    int ahead = 0, behind = 0;
    int staged = 0, changed = 0, untracked = 0, conflicts = 0;
    QString message;           // for Failed
};

virtual QJsonValue deviceSetting(const QString &key) const { ... }   // the field's default if unset
virtual QString deviceName() const { return {}; }
// The branch in <path>/.git/HEAD, following a worktree's .git file. Empty if
// detached, missing or unreadable.
virtual QString gitHead(const QString &path) const { ... }
// Runs git status in the background and calls done on the receiver's thread.
virtual void gitStatus(const QString &path, QObject *receiver, std::function<void(const GitState &)> done);
// Empty on success, else a message for the user.
virtual QString launchTerminal(const QString &dir, const QString &program, const QStringList &arguments);
virtual QString startProgram(const QString &dir, const QString &program, const QStringList &arguments);
virtual void showFolder(const QString &path);       // the file manager
virtual void mapFolder(const QString &folderId);    // asks for a local path; empty id: the main folder
virtual QString filePath(const QString &fileName) const;   // in the data folder's files/, or empty
virtual void onConfigChanged(QObject *receiver, std::function<void(const QJsonObject &)> changed);

// Status.h, in StatusJob
virtual QString folderPath(const QString &folderId) const { ... }   // empty id: the main folder
virtual QString gitHead(const QString &path) { ... }                 // also watched
```

`FakeStatusJob` gains a table of folder paths and heads. The headers written by T0 win where they differ in detail from these sketches.

## Part 2: the core, Git and commands

**`GitHead`** (`core/`): `branchAt(path)` reads `<path>/.git/HEAD`. If `.git` is a file, as in a worktree, it follows `gitdir:`. `ref: refs/heads/<name>` gives the name; a hash gives an empty branch. `headFile(path)` gives the file to watch. Pure file reading, no process.

**`GitStatusReader`** (`core/`): runs `git -C <path> status --porcelain=v2 --branch` with `QProcess`, asynchronously, with a 10 second limit. One run per path at a time; a second request for the same path waits for the first answer. `parseStatus(output)` is a pure function. Git not on the `PATH` gives `GitMissing`; exit code 128 with "not a git repository" gives `NotARepository`.

**`Launcher`** (`platform/`): `terminalCommand(platform, template, dir, program, arguments)` and `programCommand(platform, dir, program, arguments)` are pure functions that return the program and arguments to start. `launchTerminal()` and `startProgram()` start them with `QProcess::startDetached` and return a message on failure.

- The terminal defaults are those of [terminal launch](../integrations.md#terminal-launch). On Windows, `wt.exe` gets `;` escaped, because it reads `;` as a new tab. On macOS, the `cd` and the command are quoted for the shell, then for AppleScript.
- The **terminal command** device setting replaces the default. It is a command line with `{dir}` and `{command}`; `{command}` becomes the program and its arguments, each as a separate argument. Empty means the default.
- On Windows, a program whose name ends in `.cmd` or `.bat`, or is found on the `PATH` only as one, as `code` is, runs through `cmd.exe /c`, with its arguments quoted for `cmd`.

**Tests:** `tst_git_head` (branch, detached, worktree, missing), `tst_git_status` (parsing recorded output: clean, ahead and behind, each count, no upstream, detached), `tst_launcher` (the commands on each platform, a template, quoting of spaces, quotes and `;`).

## Part 3: the core, settings, contexts and status

- **`DeviceSettings`:** `terminalCommand()`, `deviceName()` (the host name until set), and `moduleSetting(moduleId, key)` / `setModuleSetting()`, under `modules/<moduleId>/<key>`.
- **`SettingsDialog`:** a **Terminal** section with the terminal command and the device name, and a **Modules** section with one group per module that has device settings, built from their fields with `FieldForm`.
- **`ItemTypeRegistry`:** keeps the handlers, finders and device setting fields of each module, in module order. T0 writes the storage.
- **`ItemContextImpl`:** the new members. `mapFolder()` opens the same folder picker as the project dialog and stores the mapping. `onConfigChanged()` is called by the placement.
- **`Placement`:** when the item changes and only keys in `viewKeys()` differ from what the view was made with, it keeps the view, updates its record of the configuration and calls the receivers of `onConfigChanged()`.
- **`StatusCache`:** its `StatusJob` implements `folderPath()` and `gitHead()`. The heads a job read are recorded with its item. A `QFileSystemWatcher` watches those files, and a change fetches the item at once, as a changed configuration does. Mapping a folder on this device fetches the project's items again.

**Tests:** `tst_device_settings` additions; `tst_placement` additions (a change to a view key keeps the view and calls the receiver; any other change makes it again); `tst_status_cache` additions (a job that reads a head is fetched again when the file changes, and not when another file does).

## Part 4: quick add in the core

**`QuickAddRun`** (`items/`), a `QObject` that does the work without widgets:

1. `propose(text, profileId)` runs every handler at once. Each has 30 seconds. It emits `proposed(index, proposal)` as answers arrive, then `proposalsDone()`.
2. `matchProjects(model, proposal, moduleId)` is a pure function. It returns the projects outside the trash whose `moduleSettings.<module>.<key>` equals the value; strings compare without case.
3. `findRelated(proposal, projectId, profileId)` runs every finder with the proposal's texts and items, and the chosen project's settings. It emits `found(items)` per finder, then `findingDone()`. Items already offered, by type and configuration, are dropped.
4. `create(projectId, title, items)` makes the task through `edits` and `Model`: state active, priority Normal, the items in order, and its dashboard with one cell that holds every item as a tab, the first one current. It returns the task id.

**`QuickAddDialog`** (`ui/`), opened by a toolbar button and `Ctrl+Shift+N`, as [ui](../ui.md#quick-add) says:

- A text box, filled from the clipboard when it holds one line. **Profile**, defaulting to the profile of the selected sidebar entry's group, else the workspace default.
- Reading starts on paste or Return, and shows "Reading…". If no handler takes the text: "Mirante does not recognise this text." If several do, a **Read as** choice lists them by module name.
- Then: **Title**, editable; **Project**, chosen when exactly one matches, otherwise empty and required, with "No project matches acme/backend" or "Several projects match"; **Items**, a list with a check box, the type's icon and the name. Finders' items join the list as they arrive, checked, under "Related". A new project choice runs the finders again.
- **Create** makes the task and opens it in the preview slot, as a click in the sidebar does. **Cancel** stops the requests that are still running.
- No project yet: the dialog says "Create a project first." and offers no Create.

**Tests:** `tst_quick_add_run` with fake handlers and finders: answers out of order, a handler that never finishes, a match on zero, one and several projects, a project in the trash left out, duplicate items dropped, the created task's items and dashboard. `tst_quick_add_dialog` for the states above, with fake runs.

## Part 5: the gitlab module

- **Handler:** a merge request address, read with the same code as `fromUrl()` against the profile's base URL. It asks for the merge request, and proposes: title = the merge request's title; items = MR summary (`!iid` as name) and CI Pipeline with `branchSource` `mergeRequest` and the same `iid`; project match `project` = the path; texts = the title and the source branch.
- **Finder:** for each Jira-style key in the texts, up to five (`[A-Za-z][A-Za-z0-9_]+-\d+`, read in upper case):
  - with a `project` setting: one request for its open merge requests, newest first, up to 100, and those whose title or source branch holds the key, without case;
  - without one: `GET /merge_requests?scope=all&state=opened&search=<key>&in=title`, which finds titles only.

  Each match is an MR summary item.
- **Pipeline following the main folder:** `fetchStatus` reads `job.folderPath("")` and `job.gitHead(path)`, then works as a `fixed` branch. Its messages, with `State::None`: "The project has no main folder.", "Set the main folder's path on this device.", and "The main folder is not on a branch." The view shows them in place of everything else, as [item types](../item-types.md#gitlab-ci-pipeline) says.

**Tests:** `tst_quick_add` on recorded replies: a merge request address, one on another server, a key in a branch, a key without a project setting, and no request for text without a key. `tst_status` replaces `mainFolderWaitsForM7` with the three messages and a branch read from a head.

## Part 6: the jira module

- **Handler:** an issue address, as `fromUrl()` reads it, or a bare key. It asks for the issue's summary, and proposes: title = the summary; items = Jira Issue (summary), named by its key; project match `projectKey` = the key's prefix; texts = the key. A `404` finishes with nothing, so text that only looks like a key is not offered.
- **Finder:** for each key in the texts, up to five, whose prefix equals the project's `projectKey`, or any key without one, it asks for the issue. A `200` offers a Jira Issue (summary).

**Tests:** `tst_quick_add` on both deployments: an address, a key, a key that is not found, a key with another prefix, and keys in a branch such as `feature/abc-123-login`.

## Part 7: the claude and folder modules

**Claude session (local)**, as in [item types](../item-types.md#claude-session-local):

- The view shows the session id, the folder's name and local path, and the device, with **Resume in terminal**.
- The button runs the `claude` device setting's program (default `claude`) with `--resume <id>`, through `launchTerminal()`, in the folder's local path.
- The button is disabled, with the reason under it, when: the item's `device` is set and differs from `deviceName()`, without case ("This session lives on <device>."); the folder has no local path ("Set the folder's path on this device.", with a **Choose folder…** button that calls `mapFolder()`); or the id holds anything but letters, digits and dashes.
- A launch failure shows its message under the button.

**Folder / terminal**, as in [item types](../item-types.md#folder--terminal):

- The view shows the folder's name and local path, the branch (or "Detached at <commit>"), and one status line such as "2 ahead · 3 changed · 1 untracked", or "Clean". `NotARepository` shows "Not a Git repository", `GitMissing` shows "Git was not found", and the actions still work.
- **Actions:** Terminal (the platform's terminal with no command), Editor (the `editor` device setting, default `code`, with the path as its last argument), File manager (`showFolder()`).
- An unmapped folder shows "Folder not set on this device" with **Choose folder…**.
- It reads the Git state when shown and every 10 seconds while visible, and draws nothing new when the state is the same.

**Tests:** `tst_views` for both, with a fake context: each disabled state and message, the program and arguments passed, an invalid id starting nothing, the Git lines for each `GitState`.

## Part 8: the notes and image modules

**Notes:**

- The view has an **Edit** and a **Preview** button in a thin bar, and a stack below. It opens in Preview when the note has text, else in Edit. `Ctrl+E` switches.
- Edit is a `QPlainTextEdit` with a fixed-width font. The text is saved with `updateConfig()` 800 ms after the last change, when the editor loses focus, and when the view is destroyed. Empty text removes the key.
- Preview is a `QTextBrowser` with `setMarkdown()`. It loads no remote resources, and opens links in the external browser.
- `viewKeys()` returns `text`. `onConfigChanged()` replaces the text only when there is no unsaved change.

**Image:**

- The image from `filePath(file)`, scaled to fit with its aspect ratio kept and smooth scaling, centred. The scaled copy is made again on resize only.
- The memo under it, as plain text, wrapped and selectable. Hidden when empty.
- With a `link`, the pointer is a hand over the image, the tooltip is the link, and a click opens it in the external browser.
- A missing file shows "The image file is missing." A new image pasted with `Ctrl+V` changes `file`, so the view is made again with it.

**Tests:** notes: saving after the pause and on destroy, empty text, an external change with and without unsaved edits, a remote image in the preview not fetched. Image: scaling keeps the ratio, the memo hides, a click with and without a link, a missing file.

## Part 9: documents

T0 updates [modules](../modules.md) (the quick add API, the new registrar, context, job and type members, and the "Since" column), [item types](../item-types.md) (`viewKeys`), [architecture](../architecture.md) for the new classes, and [sync](../sync.md#device-settings) for module device settings. The other tasks update what they change: T2 [integrations](../integrations.md#terminal-launch) for the template and quoting, T3 [ui](../ui.md#quick-add) for the dialog, T4 and T5 the gitlab and jira `README.md` files and the pipeline's "Until M7" sentence, T6 and T7 their modules' `README.md` files and [item types](../item-types.md).

## Tasks for parallel work

The rules, the roles and the brief are those of the [M2 plan](m2-app-skeleton.md#tasks-for-parallel-work), with `m2-app-skeleton.md` replaced by this plan. At most three agents run at the same time. The headers written by T0 are the interfaces.

| Task | Branch | Work | Owns | Needs |
|---|---|---|---|---|
| T0 | `m7-interfaces` | Part 1. Every new core header of parts 2 to 4 with stubs; the registry's storage; `FakeStatusJob`; stub view files and factories in the claude, folder, notes and image modules, wired to their types; stub handler and finder files in gitlab and jira, registered. Documents as in Part 9. | everything it creates; after merging, the headers belong to the orchestrator | none |
| T1 | `m7-git-and-launch` | Part 2. | `src/core/GitHead.cpp`, `src/core/GitStatusReader.cpp`, `src/platform/Launcher.cpp`, their tests | T0 |
| T2 | `m7-core` | Part 3. | `DeviceSettings.cpp`, `SettingsDialog.cpp`, `ItemContextImpl.cpp`, `Placement.cpp`, `StatusCache.cpp`, their tests | T0 |
| T3 | `m7-quick-add` | Part 4. | `src/items/QuickAddRun.cpp`, `src/ui/QuickAddDialog.cpp`, the action in `MainWindow.cpp` and `Shortcuts.cpp`, their tests | T0 |
| T4 | `m7-gitlab` | Part 5. | `src/modules/gitlab/` | T0 |
| T5 | `m7-jira` | Part 6. | `src/modules/jira/` | T0 |
| T6 | `m7-claude-folder` | Part 7. | `src/modules/claude/`, `src/modules/folder/` | T0 |
| T7 | `m7-notes-image` | Part 8. | `src/modules/notes/`, `src/modules/image/` | T0 |
| T8 | none | Done by the orchestrator: write `docs/plans/m7-checks.md`, run the `dev`, `no-modules`, `release` and `ci` presets, and tick the roadmap once the user has checked. | any | all |

Every task after T0 tests against fakes, so none waits for another's code. The order of waves only keeps three agents at a time, and puts the core first because the hand checks need it.

### Waves

| Wave | Tasks | At the same time |
|---|---|---|
| 0 | T0, then the user reviews the headers | 1 |
| 1 | T1, T2, T3 | 3 |
| 2 | T4, T5, T6 | 3 |
| 3 | T7 | 1 |
| 4 | T8 | 1 |

## Checking

- **Automatic:** the tests above, by CTest locally and in CI on Windows and Linux, with all modules and with none. No test uses the network, the real keychain, or starts a terminal. Git tests use folders made in a temporary directory, and skip the `git status` run when Git is not installed.
- **By hand on Windows,** listed in `docs/plans/m7-checks.md` by T8:
  - quick add from a gitlab.com merge request link and from a Jira Cloud key, with zero, one and two matching projects, and with related items found both ways;
  - Resume in terminal with Windows Terminal, and with `wt.exe` renamed away so the `cmd.exe` fallback runs; a session from another device; an unmapped folder;
  - the folder item in a repository, a worktree, a detached head and a plain folder; each action, with `code` as the editor;
  - a pipeline following the main folder, switching branches in a terminal while it shows;
  - a note edited, previewed, and changed from a second instance;
  - an image with a memo and a link, and one replaced with `Ctrl+V`.
- **Not checked by hand:** the macOS and Linux terminals. Their command building is tested. `Launcher` has macOS code, so the user tries it at the next manual macOS build.

## Out of scope

- Device settings for items, and with them remembered sections and column widths ([decision 0066](../decisions/0066-fixed-native-layouts-with-sections.md)).
- Suggesting a folder's local path from its remote URL, which the M3 plan left to M7 with `GitHead`. It needs a scan of the disk, and is not in M7's boxes.
- Quick add for text other than a merge request link and a Jira key or link, such as a pipeline link.
- Suggesting "done" for a task: left to M8, as the M6 plan says.
- Listing local Claude sessions from Claude Code's files. [Decision 0008](../decisions/0008-claude-sessions.md) keeps the app out of Claude Code's stored data, and the user pastes the id.
