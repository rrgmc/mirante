# 0055. Moving a workspace to another folder

- Status: Accepted
- Date: 2026-10-29

## Context

The user wants to move a workspace's data folder to another place, for example into a synced folder, without losing web logins, tokens or the state kept on this device.

Logins live in `profiles/p-<profileId>` under the app's local data folder ([decision 0042](0042-user-managed-web-profiles.md)). Tokens are keyed `<profileId>/<moduleId>/<fieldKey>` in the keychain ([decision 0054](0054-module-settings-replace-connections.md)). Neither names the workspace's folder, so both still work after a move. The [device settings](../sync.md#device-settings) that name the folder are the last workspace, the recent list, the open tabs of the workspace and project folder paths.

On Windows a folder cannot be renamed or deleted while a program holds a file or the folder itself open. The app's own folder watcher does, and so may Explorer, a terminal, a sync tool or a virus scanner.

## Decision

- **File > Move Workspace…** chooses a folder. An empty folder takes the workspace's files. Any other folder gets a new folder inside it, with the workspace folder's name. The app asks before it moves. The sample data cannot be moved.
- **Close first.** The app saves the open tabs, then closes the Store: pending writes reach the disk and the folder watcher is deleted, which releases its handles.
- **All or nothing.** The folder is first renamed in place, to `<folder>.moving-<time>`. A folder held open by another program fails here, before anything changed; the attempt is repeated for a second, for scanners that hold files briefly. Then it is renamed to the new place. Across drives, where a rename cannot work, the files are copied and the old folder removed. A failed copy removes what it copied and gives the folder its old name back. After a failure the workspace opens where it was, and the warning bar says why, naming programs that may hold it.
- **Leftovers.** If the copy succeeded but some files of the old folder could not be removed, they stay in the renamed old folder, which is no longer a workspace path the app opens. The warning bar names it.
- **Device settings follow.** The last workspace, the recent list, the saved tabs and detached windows, and project folder paths inside the old folder take the new path.
- **Logins and tokens** are left alone, since they are keyed by profile id.

## Alternatives

- **Move the files with the operating system, then open the new folder.** It works for the files, but the open tabs and project folder paths are lost, and the old path stays in the recent list.
- **Copy, then delete, always.** It works across drives, but a locked file found only during deletion leaves a half-deleted old folder.
- **Close every view before moving.** The views read files into memory and keep nothing open, so this would only cost reloading them.

## Consequences

- A workspace can move without logging in again.
- Another device that syncs the workspace keeps its own path for it; moving on one device changes only that device's settings.
- A move across drives takes as long as copying the folder, and the window waits for it.
