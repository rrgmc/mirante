# Changelog

All notable changes to Mirante are documented in this file. The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and Mirante follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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

[Unreleased]: https://github.com/rrgmc/mirante/compare/v0.9.0...HEAD
[0.9.0]: https://github.com/rrgmc/mirante/compare/v0.8.1...v0.9.0
[0.8.1]: https://github.com/rrgmc/mirante/compare/v0.8.0...v0.8.1
[0.8.0]: https://github.com/rrgmc/mirante/compare/v0.5.0...v0.8.0
[0.5.0]: https://github.com/rrgmc/mirante/releases/tag/v0.5.0
[0.1.0]: https://github.com/rrgmc/mirante/releases/tag/v0.1.0
