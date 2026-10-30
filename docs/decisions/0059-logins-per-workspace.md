# 0059. Each workspace keeps its own logins

- Status: Accepted
- Date: 2026-10-29

## Context

Each device keeps the web logins of a profile in `profiles/p-<profile id>`, in the app's local data folder ([web views](../web-views.md#profiles)). The folder is named by the profile id only. Two workspaces that list a profile with the same id therefore share its logins. Every workspace made before [decision 0057](0057-any-profile-can-be-the-default.md) has the id `default`, so the sample data and every real workspace shared one set of logins. Tokens in the keychain were keyed the same way, by profile id.

The user chose to keep each workspace's logins apart.

## Decision

- **A workspace has an id.** `workspace.mirante` holds it under `id`: a UUID, synced with the workspace. It does not change when the folder is renamed or moved ([decision 0055](0055-move-workspace.md)), or when the workspace is renamed.
- **Logins live under the workspace's id.** A profile's web data is in `profiles/<workspace id>/p-<profile id>`. The app profile of the built-in panels stays in `profiles/app`.
- **Tokens too.** A token's keychain key is `<workspace id>/<profile id>/<module id>/<field key>`.
- **New workspaces** get an id when they are made. The sample data has a fixed id of its own.
- **Workspaces from before this decision** get an id when they are first opened, written at once. The logins kept in `profiles/p-<profile id>` move into the folder of the first such workspace opened on the device, so the user stays logged in. A folder that the workspace already has is left alone.
- **A workspace without an id that cannot be written**, such as one written by a newer version, keeps its logins under a name made from its folder's path. Moving it loses them.

## Alternatives

- **Keep logins per profile id.** Simpler, and a copied workspace keeps its logins. But unrelated workspaces share logins by accident, and deleting a profile in one deletes the logins of the other.
- **Name the folder by the workspace's path.** Needs no new key, but moving the workspace, as decision 0055 allows, would lose its logins.

## Consequences

- `Model` holds the workspace id; `Store` reads it, writes it, and gives one to a workspace that has none. `Store::assignedWorkspaceId()` says when it did.
- `ProfileManager::setWorkspace()` chooses the workspace's folder. `MainWindow` calls it each time a workspace opens, before the tabs make pages, and calls `adoptRootProfiles()` when the workspace has just received its id.
- Profiles of a workspace opened earlier stay loaded until the app quits, since its pages may still be closing. Each has a Qt storage name of `<workspace id>_p-<profile id>`, unique in the process.
- A copied workspace folder keeps the id, so the copy and the original share logins on a device. Giving the copy a new id is left to the user for now, by removing `id` from its `workspace.mirante`; it then counts as a workspace from before this decision but finds no logins to take.
- Older versions keep `id` as an unknown key, and keep using `profiles/p-<profile id>`, so on a device with an older version the user logs in again there.
