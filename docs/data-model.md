# Data model

This document describes the entities, their fields and how layouts are stored. How the files are synced is in [sync](sync.md).

## Hierarchy

```
Group
└─ Project
   └─ Task
      ├─ Item         (named, typed; several of the same type allowed)
      └─ Dashboard    (task dashboard; one is the default)

Root dashboard        (outside the hierarchy; holds tasks and items from anywhere)
```

Only tasks hold items and dashboards. Groups and projects do not. Project-wide items, such as "all open merge requests of this repository", go in an ordinary task, such as one named "Overview". See [decision 0018](decisions/0018-no-default-task.md).

All ids are UUIDs, created once and never reused.

## Group

| Field | Description |
|---|---|
| `id` | UUID |
| `name` | For example "Work" |
| `order` | Manual position in the sidebar |
| `profile` | Optional. The id of the group's default [profile](#profile). Without it, or if it names no profile in the list, the group uses Default. |
| `colour` | Optional. The group's colour, as `"#rrggbb"`, such as `"#3b82f6"`. Title bars of its task dashboards and its projects in the sidebar are tinted with it. |

Groups have no priority.

A group without a `colour`, or with one that is not of the form `"#rrggbb"`, takes one of eight palette colours, chosen by a hash of its id. The choice is the same on every device, so existing groups get colours without their files changing. A new group takes the first palette colour that no group uses yet, and stores it. See [decision 0046](decisions/0046-group-colours.md). They keep their manual order.

Group files from before [decision 0042](decisions/0042-user-managed-web-profiles.md) may hold `profileOverrides`. It is no longer read, and is kept like any unknown key.

Group files from before [decision 0050](decisions/0050-profiles-hold-logins-connections-and-settings.md) name the profile `webProfile`, which is still read. They may also hold `connections` and `defaultConnections`. These move to a profile when the workspace is opened; see [migration](#migration-to-profiles).

## Project

| Field | Description |
|---|---|
| `id`, `groupId` | UUIDs |
| `name` | Display name |
| `priority` | See [priority](#priority) |
| `order` | Manual position among its siblings |
| `folders` | List of [folders](#folder); exactly one has `main: true` |
| `moduleSettings` | Project settings declared by [modules](modules.md#what-a-module-registers), as one object per module id |
| `connectionOverrides` | Optional map from connection kind to a connection id. It counts only if the item's profile has that connection. |
| `trashedAt` | Optional. When the project went to the [trash](#trash), as an ISO 8601 UTC string |

Module settings are defaults. Items in the project's tasks use them unless they set their own value. Quick add also uses them to choose the project for a pasted link. See [ui](ui.md#quick-add).

Example with the `gitlab` and `jira` modules:

```json
"moduleSettings": {
  "gitlab": { "project": "acme/backend" },
  "jira": { "projectKey": "ACME" }
}
```

Settings of a module that is not in the build are kept unchanged. See [missing modules](modules.md#missing-modules).

### Folder

A folder is stored as an identity, not as a path, because paths differ between devices.

| Field | Description |
|---|---|
| `id` | UUID |
| `name` | For example "backend" |
| `remoteUrl` | Optional Git remote URL, used to recognise the folder |
| `main` | `true` for the main folder |

Each device maps a folder id to a local path in its own settings. A device without a mapping shows "Folder not set on this device" and offers to pick one. See [decision 0010](decisions/0010-folder-identity.md).

## Task

| Field | Description |
|---|---|
| `id`, `projectId` | UUIDs |
| `title` | Display name |
| `state` | `active`, `done` or `archived` |
| `priority` | See [priority](#priority) |
| `order` | Manual position among its siblings |
| `items` | List of [items](#item) |
| `dashboards` | List of task [dashboards](#dashboard) |
| `defaultDashboardId` | The dashboard shown when the task is opened |
| `trashedAt` | Optional. When the task went to the [trash](#trash), as an ISO 8601 UTC string |

Rules:

- A new task gets one dashboard with a single empty cell.
- The user sets the state. The app suggests "done" when all linked merge requests are merged and all linked Jira issues are closed. It never changes the state on its own.

## Trash

A project or task with a `trashedAt` value is in the trash. Without the key, or with an empty value, it is not. The key is new and optional, so files without it load as before and `schemaVersion` does not change. See [decision 0036](decisions/0036-trash-for-tasks-and-projects.md).

- A project in the trash takes its tasks with it. Their own `trashedAt` is not set, so restoring the project brings back the tasks it had.
- Entries in the trash still exist. Dashboard cells keep their references to them and read "In trash", so a restore brings them back. See [normalisation](#normalisation).
- Emptying the trash, or deleting one entry permanently, deletes the entities and their files as any other deletion does.

The sidebar's Trash list and its commands are in [ui](ui.md#trash).

## Item

| Field | Description |
|---|---|
| `id` | UUID, unique within the task |
| `type` | Item type id, such as `gitlab.mr.summary`. See [item types](item-types.md). |
| `name` | Display name, shown in cell headers and tabs |
| `config` | Object with the fields of the item type |
| `connectionId` | Optional. The id of a connection of the item's profile, which replaces the profile default |
| `profile` | Optional. The id of a [profile](#profile) that replaces the group's for this item. Offered only for types that use a profile: types whose views are web views, that use connections, or whose module has settings screens. Items from before decision 0050 name it `webProfile`, which is still read. |

A task can hold any number of items of the same type, for example three Jira issues.

An item whose type belongs to a module that is not in the build is shown as a placeholder. Its `type` and `config` are kept unchanged. See [missing modules](modules.md#missing-modules).

## Profile

A profile groups what belongs to one account or context, such as "Work" or "Personal":

- its web logins, kept on each device (see [web views](web-views.md#profiles));
- its [connections](#connection), with a default for each kind, and their tokens in the keychain;
- the values of the modules' [settings screens](#module-settings).

The list belongs to the workspace and is kept in `workspace.mirante`, under `profiles`. See [decision 0050](decisions/0050-profiles-hold-logins-connections-and-settings.md), which extends [decision 0042](decisions/0042-user-managed-web-profiles.md).

| Field | Description |
|---|---|
| `id` | UUID, or `default` for Default |
| `name` | Display name. Default has none. |
| `connections` | Optional. List of [connections](#connection), without tokens |
| `defaultConnections` | Optional. For each connection kind, the id of the profile default |
| `moduleSettings` | Optional. The values of the modules' settings screens; see [module settings](#module-settings) |

```json
{
  "name": "My work",
  "profiles": [
    {
      "connections": [
        { "baseUrl": "https://gitlab.com", "fields": {}, "id": "fbba0e51-92a5-52aa-983a-bc797ee4016b",
          "kind": "gitlab", "name": "gitlab.com (personal)" }
      ],
      "defaultConnections": { "gitlab": "fbba0e51-92a5-52aa-983a-bc797ee4016b" },
      "id": "default"
    },
    { "id": "3f6c2a8e-5d1b-4c7a-9e2f-8b0d4a6c1e93", "name": "Work" }
  ],
  "schemaVersion": 1
}
```

Rules:

- **Default** always exists, with the id `default`. It is listed only when it holds connections, settings or unknown keys, and its name is not stored. An entry without an id, or with an id already listed, is skipped when the file is read.
- An item uses its own `profile`, else its group's, else Default. A step that names a profile not in the list is skipped, such as a deleted profile or one whose entry has not arrived yet. The code is `profiles::effective` in `src/core/Profiles.h`.
- Deleting a profile removes it from the list, with its connections and settings. Groups and items that name it keep the id and fall back by the rule above.
- The list, the connections and the settings are synced. The logins and the tokens are not; they stay on each device.
- Keys of a profile that this version does not know are kept.

### Migration to profiles

Before [decision 0050](decisions/0050-profiles-hold-logins-connections-and-settings.md) the list was under `webProfiles`, connections belonged to groups, and module settings were kept for the whole workspace, under `moduleSettings` at the top of `workspace.mirante`. When a workspace is opened, the app moves them, in `src/core/ProfileMigration.cpp`:

- `webProfiles` is read when there is no `profiles`, and written as `profiles`.
- The module settings at the top go to Default. A screen that Default already has keeps Default's values.
- Each group's connections go to the profile that group uses, or to Default. They keep their ids, so items and project overrides still find them. Groups are taken by their `order`, then by id. A name the profile already uses gets the group's name added, such as "gitlab.com (Personal)", and a number if that is taken too. A group default becomes the profile default when the profile has none of that kind. Otherwise each project of the group without an override for that kind gets the group default as its override.
- `workspace.mirante` is written at once, then the changed group and project files. If `workspace.mirante` cannot be written yet, the groups keep their old keys on disk, and the next load moves them again.
- A connection whose id a profile already has is skipped, so the migration can run again without changing anything. A group file with old keys that arrives later is migrated in the same way.
- Nothing is migrated while `workspace.mirante` was written by a newer version, which the app never writes over.

## Module settings

A module can have [settings screens](modules.md#settings-screens), such as GitLab's "General Configuration". Each [profile](#profile) has its own values, kept in its entry in `workspace.mirante`, under `moduleSettings`: an object of module ids, each an object of screen ids, each the values of that screen. An item uses the values of its profile. See decisions [0044](decisions/0044-module-settings-in-the-workspace.md) and [0050](decisions/0050-profiles-hold-logins-connections-and-settings.md).

```json
{
  "id": "3f6c2a8e-5d1b-4c7a-9e2f-8b0d4a6c1e93",
  "moduleSettings": {
    "gitlab": {
      "general": { "projects": [ "acme/backend", "acme/web" ] }
    }
  },
  "name": "Work"
}
```

Rules:

- The module decides the keys of each screen. The core stores them without knowing what they mean.
- Keys that the screen's form or editor does not show are kept. So are the entries of modules and screens that this build does not have.
- Saving a screen with no values removes its entry. A module left without screens is removed too, and so is `moduleSettings` when it is empty.
- The values are synced. Secrets, such as tokens, are never stored here; they stay in the keychain.

## Priority

Projects and tasks have one of four levels:

| Level | Value |
|---|---|
| Urgent | 0 |
| High | 1 |
| Normal | 2 (default) |
| Low | 3 |

The sidebar lists projects and tasks by the manual `order` field only; the level shows as a marker. Only the Focus view sorts by level. Jira priority is not used. See decisions [0009](decisions/0009-priority-and-attention.md) and [0034](decisions/0034-manual-order-in-the-sidebar.md).

Attention, such as a failed pipeline, is not a priority. It is computed from live data and shown as a badge. See [ui](ui.md#attention).

## Connection

A connection holds the details needed to call one API. Connections belong to a [profile](#profile).

| Field | Description |
|---|---|
| `id` | UUID |
| `kind` | A connection kind registered by a module, such as `gitlab`, `jira.cloud` or `jira.dc` (Data Center) |
| `name` | For example "gitlab.com (work)" |
| `baseUrl` | For example `https://gitlab.com` or `https://acme.atlassian.net` |
| `fields` | Further fields declared by the kind, such as `email` for `jira.cloud` |

The token is never part of the synced data. It is stored in the operating system keychain. See [sync](sync.md#secrets).

A profile can have several connections of the same kind. One of them is the profile default for that kind.

To find the connection for an item, the app takes the first match in this order:

1. The item's `connectionId`.
2. The project's override for that kind.
3. The profile's default for that kind.

All three must belong to the item's profile and have the right kind. See decisions [0006](decisions/0006-connections-per-group.md) and [0050](decisions/0050-profiles-hold-logins-connections-and-settings.md).

The item's profile is found by the [resolution order](#profile): its own, else its group's, else Default. The item's group is the group of the project of the item's task. An item type lists the connection kinds it can use, in order of preference, such as `jira.cloud` and then `jira.dc`. At each step the app tries these kinds in that order. Only then does it move to the next step, so a project override of the second kind wins over a profile default of the first. A step that names a connection of another kind, of another profile, or one that no longer exists is skipped. If no step matches, the item has no connection. The code is `connections::resolve` in `src/core/Connections.h`.

## Dashboard

A dashboard has an `id`, a `name` and a `layout`, which is a [layout tree](#layout-tree).

There are two kinds:

- **Task dashboards** belong to a task. Their cells show items of that task.
- **Root dashboards** stand alone. Their cells show either:
  - a task, with one of its dashboards chosen per cell; or
  - a single item from any task.

A root dashboard refers to a task's dashboard. It does not copy it. Changing the layout of a task inside a root dashboard changes that task dashboard everywhere it is shown. To show a task differently in one place, create another task dashboard and choose it in that cell. See [decision 0011](decisions/0011-root-cells-reference-task-dashboards.md).

Root dashboards have an `order` field for their position in the tab bar.

## Layout tree

A layout tree has three kinds of node, which can nest in any combination.

| Node | Fields |
|---|---|
| `split` | `dir` (`h` for side by side, `v` for stacked), `sizes` (relative numbers), `children` |
| `tabs` | `tabs` (each with `title` and `node`), `current` (index), `collapseSingle` (boolean) |
| `cell` | `ref`: an item reference, a task reference, or `null` for an empty cell |

References in cells:

- In a task dashboard: `{ "item": "<itemId>" }`.
- In a root dashboard: `{ "task": "<taskId>", "dashboard": "<dashboardId>" }` or `{ "item": "<itemId>", "task": "<taskId>" }`.

`sizes` are ratios, not pixels, so a layout looks the same on screens of different sizes.

Example: a Claude session on the left taking two thirds of the width, and on the right a tab group with two Jira issues stacked in one tab and a merge request in another.

```json
{
  "type": "split", "dir": "h", "sizes": [2, 1],
  "children": [
    { "type": "cell", "ref": { "item": "claude-1" } },
    { "type": "tabs", "current": 0, "collapseSingle": false,
      "tabs": [
        { "title": "Issues", "node": {
            "type": "split", "dir": "v", "sizes": [1, 1],
            "children": [
              { "type": "cell", "ref": { "item": "jira-1" } },
              { "type": "cell", "ref": { "item": "jira-2" } }
            ] } },
        { "title": "MR", "node": { "type": "cell", "ref": { "item": "mr-1" } } }
      ] }
  ]
}
```

### Normalisation

After every edit, the app cleans the tree:

1. A split with one child is replaced by that child.
2. A split whose child is a split in the same direction absorbs that child's children, with sizes scaled to match.
3. A tabs node with no tabs is replaced by an empty cell.
4. A tabs node with one tab keeps its tab bar, unless `collapseSingle` is `true`. Then it is replaced by the tab's node. The app no longer offers `collapseSingle` as a setting. It still reads and honours the flag in existing files. "Ungroup" in the tab menu sets it on a group of one tab, so this rule removes the group at once.
5. Removing an item from a cell leaves an empty cell, not a gap.
6. A cell that refers to a deleted item, task or dashboard becomes an empty cell showing "Removed". A task or project in the [trash](#trash) is not deleted, so a cell that refers to it, or to one of its items, keeps its reference and shows "In trash".
7. A tabs node with one tab whose node is another tabs node is replaced by that inner tabs node. A tab bar with one tab around another tab bar shows nothing new.

## What is synced

| Synced (in the shared folder) | Kept on each device |
|---|---|
| Groups, projects, tasks, items, dashboards | Local path of each folder |
| Layout trees, including size ratios | Geometry of detached root dashboard windows |
| Connection details without tokens | Open root tabs and the preview tab |
| Profiles: the list, and each group's and item's choice | Tokens (in the keychain) |
| Each profile's connections and module settings | Terminal command template |
| Notes content | |
| | Poll interval and other preferences |
| | Web logins, in each profile |

Details are in [sync](sync.md).
