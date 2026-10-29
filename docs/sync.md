# Sync

Mirante has no server. The same data appears on several devices because it is stored in a folder that an external tool keeps in step, such as Syncthing, OneDrive or Dropbox. See [decision 0004](decisions/0004-file-per-entity-sync.md).

## The data folder

The user chooses the data folder on each device. It does not have to be at the same path on every device. A data folder is a workspace only if it has `workspace.mirante` at its top. See [decision 0027](decisions/0027-workspace-needs-its-file.md).

```
<data folder>/
  workspace.mirante           marks the folder as a workspace; holds its name and web profiles
  groups/<uuid>.json
  projects/<uuid>.json
  tasks/<uuid>.json
  dashboards/<uuid>.json      root dashboards
  files/<uuid>.<ext>          images of Image items
```

- **One file per entity.** A task file holds the task's items and its task dashboards. A group file holds its connections without tokens.
- **Binary files** of items, such as the images of Image items, sit in `files/` and are referred to by name. See [decision 0021](decisions/0021-image-items-as-files.md).
- **Why this layout.** Two devices rarely change the same file at the same time. When they do, the sync tool keeps a conflict copy instead of damaging the data.
- **Version field.** Every file has a `schemaVersion` field. The app migrates older files when it loads them, and writes them back in the new version only when they change for another reason. A file with a newer `schemaVersion` was written by a newer version of Mirante. The app skips it, reports it in the warning bar, and never writes over it.
- **Unknown fields** are kept when a file is written back, so an older app version does not delete data written by a newer one.
- **Unknown modules.** Items, connections and project settings of a module that is not in this build are kept unchanged too. A device built without a module never damages its data. See [missing modules](modules.md#missing-modules).
- **Format.** Files are pretty-printed JSON with sorted keys, so the files are easy to read and compare.

A single SQLite database is not used. Sync tools copy database files whole and can damage them.

The sample data compiled into the app is read-only. Edits change what is shown, but nothing is saved. See [decision 0029](decisions/0029-sample-data-read-only.md).

## Writing

- Each change writes the affected file with `QSaveFile`, which writes to a temporary file and renames it. A reader never sees a half-written file.
- Writes are debounced by 500 ms, so dragging through a layout does not write many times.
- Deleting an entity deletes its file. Entities that refer to it are cleaned up when they are next loaded. For example, a cell refers to a deleted task: see [normalisation](data-model.md#normalisation).
- Moving a task or project to the [trash](data-model.md#trash) only changes its file, through its `trashedAt` field, so the trash syncs like any other change. Its file is deleted when the trash is emptied.
- `workspace.mirante` is never deleted. Its keys other than the name and the web profiles are kept when those change.
- A write that fails, for example because another program holds the file open for a moment, is tried again. While it fails, the warning bar shows it.

### Image files

- When an edit on this device removes the last reference to a file in `files/`, the app deletes the file. Examples are replacing an image, or deleting an item, task, project or group.
- A reference is any string in any item's configuration equal to the file name. This includes items of modules that are not in the build.
- The app never deletes a file only because it finds it unreferenced on disk. Another device may have added the image, and the task file that refers to it may not have arrived yet. A change that arrives from another device never deletes a file either.

See [decision 0030](decisions/0030-deleting-image-files.md).

## Reading external changes

- `QFileSystemWatcher` watches the top folder, for `workspace.mirante`, and its five subfolders: `groups/`, `projects/`, `tasks/`, `dashboards/` and `files/`. A subfolder that appears later is watched too.
- A change starts a reload of the changed files after 300 ms of quiet.
- The app remembers the content hash of each file it wrote, and ignores change events for files whose content matches. This avoids reloading its own writes.
- The temporary files of `QSaveFile` and of sync tools are ignored.
- A reload updates the model in place. Open views keep their state where the entity still exists. A reload is never written back.
- A root dashboard can arrive before a task it shows. Its cell then shows "Removed", but the file keeps the reference. After each reload the app reads such dashboards again, so the cell comes back when the task arrives.
- A changed file that cannot be used, for example because it is not valid JSON, is reported. The app keeps the entity as it was until the file is repaired.
- A removed file removes its entity. Removing a group or project also removes its children from the app, as loading would. If a child's file is still there, it is reported, and comes back when its parent's file does.
- A project or task whose parent is not loaded yet is reported and tried again after each later change, because a sync tool may deliver a child before its parent.
- A change in `files/` does not change the model.

## Conflict copies

Sync tools create conflict copies when two devices change the same file before syncing:

- Syncthing: `<name>.sync-conflict-<date>-<time>-<device>.json`.
- OneDrive: `<name>-<device>.json`.
- Dropbox: `<name> (<user>'s conflicted copy <date>).json`.

The app does not match these formats, because they differ between tools, versions and languages. It recognises a copy by the start of its name instead. See [decision 0031](decisions/0031-conflict-copies-by-id-prefix.md).

- Entity files and images are named after a UUID. A file whose name starts with a UUID, but is not exactly `<uuid>.json` or `<uuid>.<ext>`, is a conflict copy of the file with that UUID.
- The original has the extension that ends the copy's name. For example, `<uuid>.sync-conflict-20261028-101500-ABCDEFG.json` is a copy of `<uuid>.json`.
- A file whose name starts with `workspace` and ends with `.mirante`, other than `workspace.mirante`, is a conflict copy of `workspace.mirante`.

The app does not load conflict copies. It lists them in a warning bar with three choices:

- keep the current file;
- use the copy instead;
- show both side by side, with the lines that differ marked. JSON is shown pretty-printed, with keys, strings, numbers and `true`, `false` and `null` in their own colours.

The chosen loser is deleted.

## Device settings

Settings that only make sense on one device are stored with `QSettings`, outside the data folder:

- the data folder path: the last workspace opened, and the list of recent ones;
- the local path of each project [folder](data-model.md#folder);
- the main window's size, whether it is maximized, and the sidebar width;
- detached root dashboard window geometry;
- the open root tabs, the current tab and the detached root dashboards of each workspace;
- terminal command template, editor command;
- poll interval, the hidden-view limit;
- the global hotkey;
- the device name, shown on local Claude session items.

Started with `--settings <file>`, the app keeps these settings in that INI file instead, and leaves the device's own settings alone. Test runs use it, so they do not change the last workspace, the recent list or the open tabs of the person using the app.

## Secrets

- Tokens are stored in the operating system keychain through the qtkeychain library:
  - Windows Credential Manager;
  - macOS Keychain;
  - Secret Service (libsecret) on Linux.
- Each token is stored under service `mirante` and key `<groupId>/<connectionId>`. The key contains no secret, so it can be derived from synced data.
- Tokens are never written to the data folder and never synced. Each device asks for each token once, when an item first needs it. See [decision 0006](decisions/0006-connections-per-group.md).
- Web logins are not synced either. The list of web profiles is synced, in `workspace.mirante`, but the user logs in once per profile on each device. See [web views](web-views.md#profiles).
