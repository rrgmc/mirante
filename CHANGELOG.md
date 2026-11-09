# Changelog

All notable changes to Mirante are documented in this file. The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and Mirante follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html). A pull request does not edit this file. It adds a fragment to [changelog.d](changelog.d/README.md), and the release moves the fragments here. See [decision 0154](https://github.com/rrgmc/mirante-core/blob/master/docs/decisions/0154-changelog-fragments.md) in the core.

## [Unreleased]

## [0.14.0] - 2026-11-09

Built from the core's v0.14.0, `mirante-modules` v0.13.0 and `mirante-modules-work` v0.12.0.

### Changed

- A double click on an item's title opens the item's dialog, in every dashboard. In a task cell, a double click opens the task dashboard only on the task's own title. Before, a double click anywhere in the cell did.

### Fixed

- On macOS, Mirante hidden to the menu bar shows again on the current desktop (Space). Before, macOS switched back to the desktop the window was hidden from.

## [0.13.0] - 2026-11-05

Built from the core's v0.13.0, `mirante-modules` v0.13.0 and `mirante-modules-work` v0.12.0.

### Changed

- **Collapse All Tasks** works in two steps: the first press closes every task, and a second press, while every task is closed, closes the projects too. **Expand All Tasks** is the reverse. The sidebar's header buttons do the same.
- On macOS, a left click on the menu bar icon shows or hides Mirante. A right click or a Control-click opens the menu.

### Fixed

- The colour dot of the current dashboard tab keeps its colour. Before, it turned white.
- In lists such as Jira issues or GitLab merge requests, a long title no longer hides the other columns. Status pills no longer cut a word short on macOS.

## [0.12.0] - 2026-11-04

Built from the core's v0.12.0, `mirante-modules` v0.12.0 and `mirante-modules-work` v0.12.0.

### Added

- The System module, with the System Resources item type: this computer's CPU use, free memory and free disk space, each with a bar, read again every few seconds.
- Claude agents, a list of the Claude Code sessions on this device, with their title, status and repository. The optional `mirante-bridge` Claude Code plugin adds each session's tool, model, context use and cost, says when a session waits for a permission, and shows each account's rate limits.
- GitHub Status and GitLab Status show the health of github.com and gitlab.com: open incidents, planned maintenance and the components that do not work. They need no token.
- A profile can hold several accounts of a module, such as a personal GitHub account and a bot, each with its own settings and token. An item chooses its account in the item dialog.
- **Duplicate item…** in the context menu of an item in the sidebar, and **Duplicate Item…** in the menu of an item cell, open the new item dialog filled with a copy of the item.
- Item cells have an **Open in browser** button in their header when the item has a page on the web. It replaces the Web buttons of the GitHub, GitLab and Jira views and the button in a web page's toolbar.

### Changed

- **Profiles > Manage Profiles…** is now the one place for profile settings: a tree of profiles, their modules and the modules' accounts. The Modules, Set as Default and Settings… submenus of each profile are removed.
- Every build has **Help > Show Log**.
- **Add Items from Link…** and **New Task from Link…** take any web address. When no module recognises it, they offer an item of the type that shows it best, such as a Web page.

### Removed

- **Help > Open Sample Data**. The sample data still opens at start when there is no workspace.
- **Paste URL…** in the context menu of a task and of its Items row. Use **Add Items from Link…** instead.

### Fixed

- A module's section in the project dialog has a **Remove** button.
- On macOS, cell and page headers no longer cut a title that fits short with an ellipsis.

## [0.11.0] - 2026-11-04

Built from the core's v0.8.6, `mirante-modules` v0.8.1 and `mirante-modules-work` v0.11.0.

### Added

- The GitHub and GitLab settings have an **Add from Server…** button under their lists of repositories and project paths. It lists those your token can reach, to check and add, so you need not type them.
- The GitHub and GitLab lists over several repositories or projects have an **Add from List…** button. It offers the repositories and projects of the profile's settings, to check and add.

### Changed

- A GitHub, GitLab or Jira list keeps your last filter choice and chooses it again at the next start. The item's form no longer has a "Filters at the start" group.

## [0.10.0] - 2026-11-03

Built from the core's v0.8.5, `mirante-modules` v0.8.0 and `mirante-modules-work` v0.10.0.

### Added

- New GitHub and GitLab lists over several repositories or projects: pull requests, runs, merge requests and pipelines. Each row shows its repository or project, and the table has a filter on it.
- A GitHub or GitLab list's filters can start with values from the item's settings: status, workflow, repository and project.

## [0.9.0] - 2026-11-03

Built from the core's v0.8.5, `mirante-modules` v0.8.0 and `mirante-modules-work` v0.9.0.

### Changed

- A new workspace starts with one group, "Default", so the first project has somewhere to go.
- The Status filter of the GitHub, GitLab and Jira lists can choose several values. Clicking a value checks or unchecks it and leaves the list open; clicking "All" clears them.
- The GitHub, GitLab and Jira summaries and lists are drawn by layouts that a build of your own can replace. Mirante's own layouts draw them as before.

### Fixed

- The Windows setup's last page shows the texts of its two checkboxes again. The "Start Mirante when I sign in" box starts unticked on a first install.

## [0.8.1] - 2026-11-03

### Changed

- Help > About lists each module by its name and the release it came from, such as "Claude: 0.8.0".
- The Windows setup no longer starts Mirante when you sign in, unless you tick the box on its last page. An upgrade keeps your choice.

### Fixed

- On Windows, the window no longer seems to close and open again the first time a tab with a web page opens.
- The Windows setup's last page is in Portuguese when Windows is, as its other pages are.

## [0.8.0] - 2026-11-03

### Added

- Help > About lists each module of the build, with the repository and the commit it was built from.
- Each module carries its own translations, so a module's texts are in Brazilian Portuguese as the rest of the app is.

### Changed

- Mirante is now built from its core and its modules, which are released on their own and pinned here at their release tags. The app, its settings and its workspaces are the same as before. See [BUILDING.md](BUILDING.md).
- The macOS disk image is signed and notarised in a step of its own, after the build and its tests. It opens without a warning, as before.
- Web pages that a module ships with itself each have an origin of their own, so one module's page cannot read another's stored data.

### Removed

- Item web views no longer show `qrc:` addresses, whose pages all shared one origin.

## [0.5.0] - 2026-11-01

Links and File tree item types, a fix for starting Mirante a second time on Windows, and work on the build and CI. See the [release notes](https://github.com/rrgmc/mirante/releases/tag/v0.5.0).

## [0.1.0] - 2026-10-30

The first release. See the [release notes](https://github.com/rrgmc/mirante/releases/tag/v0.1.0).

[Unreleased]: https://github.com/rrgmc/mirante/compare/v0.14.0...HEAD
[0.14.0]: https://github.com/rrgmc/mirante/compare/v0.13.0...v0.14.0
[0.13.0]: https://github.com/rrgmc/mirante/compare/v0.12.0...v0.13.0
[0.12.0]: https://github.com/rrgmc/mirante/compare/v0.11.0...v0.12.0
[0.11.0]: https://github.com/rrgmc/mirante/compare/v0.10.0...v0.11.0
[0.10.0]: https://github.com/rrgmc/mirante/compare/v0.9.0...v0.10.0
[0.9.0]: https://github.com/rrgmc/mirante/compare/v0.8.1...v0.9.0
[0.8.1]: https://github.com/rrgmc/mirante/compare/v0.8.0...v0.8.1
[0.8.0]: https://github.com/rrgmc/mirante/compare/v0.5.0...v0.8.0
[0.5.0]: https://github.com/rrgmc/mirante/releases/tag/v0.5.0
[0.1.0]: https://github.com/rrgmc/mirante/releases/tag/v0.1.0
