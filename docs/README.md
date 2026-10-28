# Mirante documentation

Mirante is a personal desktop dashboard for all of one person's work, built with Qt 6. Start with the [overview](overview.md).

## Documents

| Document | Contents |
|---|---|
| [Overview](overview.md) | Vision, platforms, scope and non-goals |
| [Data model](data-model.md) | Entities, fields, priorities, connections, layout trees |
| [User interface](ui.md) | Navigation, dashboards, layout editing, Focus, keyboard, tray |
| [Item types](item-types.md) | Every type of panel: configuration, display, actions |
| [Modules](modules.md) | How item types are grouped, built, enabled and contributed |
| [Integrations](integrations.md) | GitLab, Jira, Claude, polling, terminal launch |
| [Web views](web-views.md) | Login profiles, live copies, memory, built-in panels |
| [Sync](sync.md) | Data folder, conflict copies, device settings, secrets |
| [Architecture](architecture.md) | Modules, classes, data flow, build, tests |
| [Roadmap](roadmap.md) | Milestones with acceptance criteria |
| [Plans](plans/) | Detailed code plans, one per milestone, written just before it starts |
| [Decisions](decisions/README.md) | Records of design decisions and their reasons |

## Glossary

**Group.** The top level, such as "Personal" or "Work". A group holds projects, [connections](data-model.md#connection) and web login settings.

**Project.** A unit of work inside a group, usually one repository. A project has a main folder, optional extra folders and a priority.

**Folder.** A directory that belongs to a project, usually a Git repository. Synced data stores its name and Git remote URL. Each device maps it to a local path.

**Task.** A piece of work inside a project. A task has items, dashboards, a state (active, done or archived) and a priority.

**Item.** One named panel in a task, such as one merge request or one Jira list. A task can have several items of the same type.

**Item type.** What kind of panel an item is, such as "GitLab MR (summary)" or "Jira List". See [item types](item-types.md).

**Module.** A folder of item types and the code they share, such as connection kinds and API clients. Each module can be included or left out when building. See [modules](modules.md).

**Connection kind.** The type of a connection, such as `gitlab` or `jira.cloud`. A module registers it and decides how requests are signed.

**Placement.** One place where an item is shown: a cell, or an item tab. Each visible placement has its own view, unless the item type allows only one.

**Dashboard.** A named arrangement of items or tasks, stored as a layout tree.
- A **task dashboard** belongs to a task and shows its items.
- A **root dashboard** is a tab in the main window and shows tasks and single items from anywhere.

**Layout tree.** The structure of a dashboard, made of splits, tabs and cells nested in any combination. See [data model](data-model.md#layout-tree).

**Split.** A layout node that places its children side by side or stacked, with sizes the user can drag.

**Tabs.** A layout node that shows one of its children at a time, chosen by a tab bar.

**Cell.** A layout node that shows one item or one task, or nothing (an empty cell).

**Preview tab.** A temporary tab that shows the last task clicked in the sidebar. The next click replaces it, unless it is pinned.

**Focus view.** A list of active tasks from all projects, sorted by priority.

**Priority.** Urgent, High, Normal or Low, set on projects and tasks.

**Attention.** A live reason to act now, such as a failed pipeline. Shown as a badge. It is not a priority.

**Connection.** The details for calling one API: kind, base URL and the kind's own fields, such as an email for Jira Cloud. The token is kept in the keychain. Connections belong to a group.

**Profile.** A web login storage (cookies and site data). Each group has one, and sites can be sent to a shared profile instead.

**Status cache.** The part of the app that polls GitLab and Jira, keeps the results and computes attention.

**Data folder.** The folder holding the synced JSON files.
