# Sync

Mirante has no server. The same data appears on several devices because it is stored in a folder that an external tool keeps in step, such as Syncthing, OneDrive or Dropbox. See [decision 0004](decisions/0004-file-per-entity-sync.md).

## The data folder

The user chooses the data folder on each device. It does not have to be at the same path on every device. A data folder is a workspace only if it has `workspace.mirante` at its top. See [decision 0027](decisions/0027-workspace-needs-its-file.md).

```
<data folder>/
  workspace.mirante           marks the folder as a workspace; holds its id, name and profiles, with their module settings
  groups/<uuid>.json
  projects/<uuid>.json
  tasks/<uuid>.json
  dashboards/<uuid>.json      root dashboards
  files/<uuid>.<ext>          images of Image items
```

- **One file per entity.** A task file holds the task's items and its task dashboards. The profiles, with their module settings and without tokens, are in `workspace.mirante`.
- **Binary files** of items, such as the images of Image items, sit in `files/` and are referred to by name. See [decision 0021](decisions/0021-image-items-as-files.md).
- **Why this layout.** Two devices rarely change the same file at the same time. When they do, the sync tool keeps a conflict copy instead of damaging the data.
- **Version field.** Every file has a `schemaVersion` field. The app migrates older files when it loads them, and writes them back in the new version only when they change for another reason. The one exception is the move of connections into module settings of profiles, which changes several files together and is written once when the workspace opens; see [migration to profiles](data-model.md#migration-to-profiles). A file with a newer `schemaVersion` was written by a newer version of Mirante. A workspace with such a file is not opened: the warning bar says to update Mirante, and whatever was open stays open. See [decision 0074](decisions/0074-refuse-workspaces-from-newer-versions.md). If such a file arrives while the workspace is open, the app skips it, reports it in the warning bar, and never writes over it.
- **Unknown fields** are kept when a file is written back, so an older app version does not delete data written by a newer one.
- **Unknown modules.** Items, project settings and module settings of a module that is not in this build are kept unchanged too. A device built without a module never damages its data. See [missing modules](modules.md#missing-modules).
- **Format.** Files are pretty-printed JSON with sorted keys, so the files are easy to read and compare.

A single SQLite database is not used. Sync tools copy database files whole and can damage them.

The sample data compiled into the app is read-only. Edits change what is shown, but nothing is saved. See [decision 0029](decisions/0029-sample-data-read-only.md).

## Writing

- Each change writes the affected file with `QSaveFile`, which writes to a temporary file and renames it. A reader never sees a half-written file.
- Writes are debounced by 500 ms, so dragging through a layout does not write many times.
- Deleting an entity deletes its file. Entities that refer to it are cleaned up when they are next loaded. For example, a cell refers to a deleted task: see [normalisation](data-model.md#normalisation).
- Moving a task or project to the [trash](data-model.md#trash) only changes its file, through its `trashedAt` field, so the trash syncs like any other change. Its file is deleted when the trash is emptied.
- `workspace.mirante` is never deleted. Its keys other than the name and the profiles are kept when those change.
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
- the main window's place and size, whether it is maximized, and the sidebar width;
- the options of the [settings window](ui.md#settings), such as the sidebar's hover tint;
- detached root dashboard window geometry;
- the open root tabs, the current tab and the detached root dashboards of each workspace;
- the workspace id this device last opened each workspace folder with ([decision 0062](decisions/0062-logins-follow-a-new-workspace-id.md));
- the terminal command template ([terminal launch](integrations.md#terminal-launch));
- the device settings that modules declare, such as the `folder` module's editor command, under `modules/<module id>/<key>` ([decision 0079](decisions/0079-module-device-settings.md));
- the status poll interval, from 30 seconds to one hour, two minutes if not set ([polling](integrations.md#polling-and-caching)), and the hidden-view limit;
- the attention reasons seen on each item, by workspace id and item id: the keys of the reasons that clear when seen ([decision 0061](decisions/0061-attention-from-status-of-any-rendering.md)). They are not synced, so each device shows an event until it is seen there. Removing an item removes its record;
- whether Focus lists the tasks that need attention first;
- the global hotkey, and whether closing the window hides it to the tray;
- the language, under `language`: `system`, the default and not stored, `en` or `pt_BR`. It is read at start, before any window is made ([decision 0086](decisions/0086-english-and-brazilian-portuguese.md)).

Moving a workspace with File > Move Workspace… updates the settings that name its folder: the last workspace, the recent list, its open tabs, its remembered id, and project folder paths inside it. See [decision 0055](decisions/0055-move-workspace.md).

Started with `--settings <file>`, the app keeps these settings in that INI file instead, and leaves the device's own settings alone. Test runs use it, so they do not change the last workspace, the recent list or the open tabs of the person using the app.

## Secrets

- Tokens are stored in the operating system keychain through the qtkeychain library:
  - Windows Credential Manager;
  - macOS Keychain;
  - Secret Service (libsecret) on Linux.
- A token is entered on the module's settings screen, in a field of type `Secret`. It is stored under service `mirante` and key `<workspaceId>/<profileId>/<moduleId>/<fieldKey>`, such as `5a3e…/default/gitlab/token`, so each workspace keeps its own ([decision 0059](decisions/0059-logins-per-workspace.md)). The keychain entry is named `mirante/` followed by the key, so Windows Credential Manager, which shows only the name, says whose it is ([decision 0064](decisions/0064-keychain-entries-start-with-mirante.md)). The key contains no secret, so it can be derived from synced data. The screen says whether the token is set on this device, and its **Remove** button removes it. See [ui](ui.md#modules).
- Tokens are never written to the data folder and never synced. Each device asks for each token once. See [decision 0054](decisions/0054-module-settings-replace-connections.md).
- Tokens are stored from M5. No earlier version stored a token, so there are no old keys to move. The log never holds a token's value.
- The app keeps each token in memory after the first read, so it asks the keychain once per key in a session.
- macOS asks for permission before an app reads a keychain entry. **Always Allow** remembers the app's signature. An ad hoc signature changes with each build, so set `MIRANTE_MACOS_SIGN_IDENTITY` to keep the answer across builds. See [decision 0087](decisions/0087-sign-the-macos-bundle.md).
- A keychain error, such as a locked keychain or no Secret Service on Linux, shows "Could not use the keychain: <message>" in the main window's warning bar. The token then counts as not set.
- With `--no-keychain`, tokens are kept in memory only and are lost at exit. Tests, and checks run with a scratch settings file, use it so they never touch the user's keychain.
- The keychain cannot list its keys. To remove or move tokens, the app builds every key a workspace can have: each profile in `workspace.mirante`, with each `Secret` field of the modules' settings screens in this build. Deleting a profile removes its tokens on this device.
- When another device changed the workspace id, tokens move to the new id with the logins. A token already under the new key wins, and the old one is removed. Tokens of modules not in this build stay under the old key. Requests wait until the move is done. See [decision 0063](decisions/0063-tokens-follow-a-new-workspace-id.md).
- Web logins are not synced either. The list of profiles is synced, in `workspace.mirante`, but the user logs in once per profile on each device. See [web views](web-views.md#profiles).
- Module settings are synced, in each profile in `workspace.mirante`, so they never hold a token or a password. See [decision 0044](decisions/0044-module-settings-in-the-workspace.md).
