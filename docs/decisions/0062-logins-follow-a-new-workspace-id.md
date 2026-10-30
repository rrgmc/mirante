# 0062. Logins follow a workspace id that another device changed

- Status: Accepted
- Date: 2026-10-29

## Context

[Decision 0059](0059-logins-per-workspace.md) keeps a workspace's logins under its id, in `profiles/<workspace id>/p-<profile id>`. A workspace without an id gets one when it is first opened, written at once.

Two devices can do this at nearly the same time. One device writes its id. Before the sync service delivers that file, a second device opens its older copy, finds no id, and writes another one. The second file wins. The first device then opens the workspace under an id it has never seen, and every page is logged out. Its logins stay on disk under the old id, unused. This happened on the first day of decision 0059, between a Windows and a macOS device sharing a workspace through Google Drive.

## Decision

- **Each device remembers the id.** The device settings keep, for each workspace folder, the id that device last opened it with.
- **A changed id takes the old logins.** When a workspace opens with an id read from its file that differs from the remembered one, the profile folders of the remembered id move to the new id. Then the remembered id is updated. A folder the workspace already has under the new id is left alone, as with [decision 0059](0059-logins-per-workspace.md)'s move of older logins. The old id's folder is removed once it is empty.
- **An id this device gives takes nothing.** When the store gives the workspace a new id, the old id's logins stay where they are. So removing `id` from a copied workspace still gives the copy a fresh start, as decision 0059 describes.
- **Moving the folder** with File > Move Workspace… also moves the remembered id to the new path ([decision 0055](0055-move-workspace.md)).

## Alternatives

- **Prevent two ids.** The id could be made from something both devices share, such as the first profile's id, so that both would make the same one. Workspaces from before decision 0059 have no such shared value that is stable, and a device can always open an older copy. The race can only be made harmless, not impossible.
- **Keep the smaller of two ids.** A device that sees an id differing from its own could write back the smaller one. Both devices would agree in the end, but each disagreement rewrites a synced file, and the device that loses still needs to move its logins.
- **Ask the user.** The user cannot tell which id is right, and either answer means moving folders.

## Consequences

- `DeviceSettings::workspaceId()` and `setWorkspaceId()` keep the remembered id, by a hash of the folder's path. `ProfileManager::adoptProfilesOf()` moves the folders. `MainWindow` calls both each time a workspace opens, before the tabs make pages.
- The move happens only when a workspace opens. If the id changes through a sync while the workspace is open, pages keep the old folder until the workspace is opened again.
- Tokens in the keychain are keyed by the workspace id too, and do not move: the keychain cannot list its keys. After another device changes the id, the user enters the tokens again on this device.
- A device that never opened the workspace under its old id has nothing to move, as before.
