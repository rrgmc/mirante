# Data model

This document describes the entities, their fields and how layouts are stored. How the files are synced is in [sync](sync.md).

## Hierarchy

```
Group
└─ Project
   └─ Task            (every project has one Default task)
      ├─ Item         (named, typed; several of the same type allowed)
      └─ Dashboard    (task dashboard; one is the default)

Root dashboard        (outside the hierarchy; holds tasks and items from anywhere)
```

Only tasks hold items and dashboards. Groups and projects do not. Project-wide items, such as "all open merge requests of this repository", belong in the project's Default task.

All ids are UUIDs, created once and never reused.

## Group

| Field | Description |
|---|---|
| `id` | UUID |
| `name` | For example "Work" |
| `order` | Manual position in the sidebar |
| `connections` | List of [connections](#connection) |
| `defaultConnections` | For each connection kind, the id of the group default |
| `profileOverrides` | Map from site host to `"shared"`. See [web views](web-views.md#profiles). |

Groups have no priority. They keep their manual order.

## Project

| Field | Description |
|---|---|
| `id`, `groupId` | UUIDs |
| `name` | Display name |
| `priority` | See [priority](#priority) |
| `order` | Manual position within its priority level |
| `folders` | List of [folders](#folder); exactly one has `main: true` |
| `gitlabProject` | Optional GitLab project path, such as `acme/backend` |
| `jiraProjectKey` | Optional Jira project key, such as `ACME` |
| `connectionOverrides` | Optional map from connection kind to a connection id of the same group |

`gitlabProject` and `jiraProjectKey` are defaults. Items in the project's tasks use them unless they set their own value. Quick add also uses them to choose the project for a pasted link. See [ui](ui.md#quick-add).

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
| `isDefault` | `true` for the project's Default task |
| `state` | `active`, `done` or `archived` |
| `priority` | See [priority](#priority) |
| `order` | Manual position within its priority level |
| `items` | List of [items](#item) |
| `dashboards` | List of task [dashboards](#dashboard) |
| `defaultDashboardId` | The dashboard shown when the task is opened |

Rules:

- Every project has exactly one Default task. It cannot be deleted, renamed away from "Default", or archived.
- A new task gets one dashboard with a single empty cell.
- The user sets the state. The app suggests "done" when all linked merge requests are merged and all linked Jira issues are closed. It never changes the state on its own.

## Item

| Field | Description |
|---|---|
| `id` | UUID, unique within the task |
| `type` | Item type id, such as `gitlab.mr.summary`. See [item types](item-types.md). |
| `name` | Display name, shown in cell headers and tabs |
| `config` | Object with the fields of the item type |
| `connectionId` | Optional connection override |

A task can hold any number of items of the same type, for example three Jira issues.

## Priority

Projects and tasks have one of four levels:

| Level | Value |
|---|---|
| Urgent | 0 |
| High | 1 |
| Normal | 2 (default) |
| Low | 3 |

Lists sort by level first, then by the manual `order` field. Jira priority is not used. See [decision 0009](decisions/0009-priority-and-attention.md).

Attention, such as a failed pipeline, is not a priority. It is computed from live data and shown as a badge. See [ui](ui.md#attention).

## Connection

A connection holds the details needed to call one API. Connections belong to a group.

| Field | Description |
|---|---|
| `id` | UUID |
| `kind` | `gitlab`, `jira-cloud` or `jira-dc` (Data Center) |
| `name` | For example "gitlab.com (work)" |
| `baseUrl` | For example `https://gitlab.com` or `https://acme.atlassian.net` |
| `email` | Jira Cloud only; used with the API token |

The token is never part of the synced data. It is stored in the operating system keychain. See [sync](sync.md#secrets).

A group can have several connections of the same kind. One of them is the group default for that kind.

To find the connection for an item, the app takes the first match in this order:

1. The item's `connectionId`.
2. The project's override for that kind.
3. The group's default for that kind.

All three must belong to the item's group and have the right kind. See [decision 0006](decisions/0006-connections-per-group.md).

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
4. A tabs node with one tab keeps its tab bar, unless `collapseSingle` is `true`. Then it is replaced by the tab's node.
5. Removing an item from a cell leaves an empty cell, not a gap.
6. A cell that refers to a deleted item, task or dashboard becomes an empty cell showing "Removed".

## What is synced

| Synced (in the shared folder) | Kept on each device |
|---|---|
| Groups, projects, tasks, items, dashboards | Local path of each folder |
| Layout trees, including size ratios | Geometry of detached windows |
| Connection details without tokens | Open root tabs and the preview tab |
| Profile overrides | Tokens (in the keychain) |
| Notes content | Terminal command template |
| | Poll interval and other preferences |

Details are in [sync](sync.md).
