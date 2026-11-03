# Changelog

All notable changes to Mirante are documented in this file. The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and Mirante follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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

[Unreleased]: https://github.com/rrgmc/mirante/compare/v0.8.0...HEAD
[0.8.0]: https://github.com/rrgmc/mirante/compare/v0.5.0...v0.8.0
[0.5.0]: https://github.com/rrgmc/mirante/releases/tag/v0.5.0
[0.1.0]: https://github.com/rrgmc/mirante/releases/tag/v0.1.0
