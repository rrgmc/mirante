# Sync

rraidash has no server. The same data appears on several devices because it is stored in a folder that an external tool keeps in step, such as Syncthing, OneDrive or Dropbox. See [decision 0004](decisions/0004-file-per-entity-sync.md).

## The data folder

The user chooses the data folder on each device. It does not have to be at the same path on every device.

```
<data folder>/
  groups/<uuid>.json
  projects/<uuid>.json
  tasks/<uuid>.json
  dashboards/<uuid>.json      root dashboards
```

- **One file per entity.** A task file holds the task's items and its task dashboards. A group file holds its connections without tokens.
- **Why this layout.** Two devices rarely change the same file at the same time. When they do, the sync tool keeps a conflict copy instead of damaging the data.
- **Version field.** Every file has a `schemaVersion` field. The app migrates older files when it loads them, and writes them back in the new version only when they change for another reason.
- **Unknown fields** are kept when a file is written back, so an older app version does not delete data written by a newer one.
- **Unknown modules.** Items, connections and project settings of a module that is not in this build are kept unchanged too. A device built without a module never damages its data. See [missing modules](modules.md#missing-modules).
- **Format.** Files are pretty-printed JSON with sorted keys, so the files are easy to read and compare.

A single SQLite database is not used. Sync tools copy database files whole and can damage them.

## Writing

- Each change writes the affected file with `QSaveFile`, which writes to a temporary file and renames it. A reader never sees a half-written file.
- Writes are debounced by 500 ms, so dragging through a layout does not write many times.
- Deleting an entity deletes its file. Entities that refer to it are cleaned up when they are next loaded. For example, a cell refers to a deleted task: see [normalisation](data-model.md#normalisation).

## Reading external changes

- `QFileSystemWatcher` watches the four subfolders.
- A change starts a reload of the changed files after 300 ms of quiet.
- The app remembers the content hash of each file it wrote, and ignores change events for files whose content matches. This avoids reloading its own writes.
- A reload updates the model in place. Open views keep their state where the entity still exists.

## Conflict copies

Sync tools create conflict copies when two devices change the same file before syncing:

- Syncthing: `<name>.sync-conflict-<date>-<time>-<device>.json`.
- OneDrive and Dropbox add a device name or "conflicted copy" to the file name.

The app does not load conflict copies. It lists them in a warning bar with three choices:

- keep the current file;
- use the copy instead;
- show both side by side.

The chosen loser is deleted.

## Device settings

Settings that only make sense on one device are stored with `QSettings`, outside the data folder:

- the data folder path;
- the local path of each project [folder](data-model.md#folder);
- detached window geometry;
- open root tabs and the preview tab;
- terminal command template, editor command;
- poll interval, the hidden-view limit;
- the global hotkey;
- the device name, shown on local Claude session items.

## Secrets

- Tokens are stored in the operating system keychain through the qtkeychain library:
  - Windows Credential Manager;
  - macOS Keychain;
  - Secret Service (libsecret) on Linux.
- Each token is stored under service `rraidash` and key `<groupId>/<connectionId>`. The key contains no secret, so it can be derived from synced data.
- Tokens are never written to the data folder and never synced. Each device asks for each token once, when an item first needs it. See [decision 0006](decisions/0006-connections-per-group.md).
- Web login profiles are not synced either. The user logs in once per device and group. See [web views](web-views.md#profiles).
