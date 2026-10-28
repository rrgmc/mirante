# Overview

## Vision

Mirante is a personal desktop dashboard for all of one person's work. It collects everything a piece of work needs in one place: Claude sessions, GitLab merge requests and pipelines, Jira issues, web pages, notes and project folders.

Work is organised in four levels:

1. **Groups**, such as "Personal" and "Work".
2. **Projects** inside a group. A project usually maps to one repository.
3. **Tasks** inside a project. Every project also has a Default task.
4. **Items** inside a task. An item is one typed panel, such as one merge request or one Jira list.

Items are arranged on **dashboards**. A dashboard is a layout of splits, tabs and cells that the user can change freely. Each task has its own dashboards. Root dashboards combine several tasks, or single items, from anywhere.

See [the glossary](README.md#glossary) for exact definitions.

## Platforms and stack

- Windows, macOS and Linux.
- Qt 6.8 LTS with Qt Widgets, written in C++17.
- CMake as the build system.
- Qt WebEngine for web content, Qt WebChannel for the bridge to built-in HTML panels.
- qtkeychain for secrets, QHotkey for the global hotkey.

The reasons for choosing Qt are in [decision 0001](decisions/0001-qt6-widgets.md).

## Scope of the first version

- The full hierarchy: groups, projects, tasks and items, with priorities and task states.
- Task dashboards and root dashboards with freely nested splits and tabs.
- Detachable panels in separate operating system windows.
- Web views with one login profile per group.
- Native views of GitLab and Jira data, with light actions.
- Claude sessions on claude.ai in a web view, and local Claude Code sessions started in a terminal.
- Sync between devices through a folder that an external tool syncs.
- System tray, notifications for failed pipelines, keyboard navigation and a global hotkey.
- Quick add: create a task from a pasted Jira key or merge request URL.

The order of work is in the [roadmap](roadmap.md).

## Non-goals

- **No central server.** Sync uses a folder that Syncthing, OneDrive or Dropbox keeps in step. See [sync](sync.md).
- **No unofficial Claude APIs.** Claude Remote Control and cloud sessions are only opened as web pages. See [decision 0008](decisions/0008-claude-sessions.md).
- **No floating windows inside the main window.** The main window only uses splits and tabs. Panels can still be detached into separate windows.
- **No team features.** Mirante is for one person. Jira priority, for example, is ignored on purpose.

## Documents

- [Data model](data-model.md): entities, fields, layout trees.
- [User interface](ui.md): navigation, dashboards, editing, keyboard.
- [Item types](item-types.md): every type of panel.
- [Integrations](integrations.md): GitLab, Jira, Claude, polling.
- [Web views](web-views.md): profiles, memory, built-in panels.
- [Sync](sync.md): synced folder, device settings, secrets.
- [Architecture](architecture.md): modules, classes, build.
- [Roadmap](roadmap.md): milestones.
- [Decisions](decisions/README.md): why things are the way they are.
