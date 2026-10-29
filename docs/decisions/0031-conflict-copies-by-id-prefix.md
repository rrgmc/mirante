# 0031. Conflict copies are recognised by the id at the start of their name

- Status: Proposed
- Date: 2026-10-28

## Context

Sync tools name conflict copies in different ways. Syncthing adds `.sync-conflict-<date>-<time>-<device>`, Dropbox adds `(<device>'s conflicted copy <date>)`, and OneDrive adds the device name. The formats also change between versions and languages.

## Decision

Every entity file and image file is named after a UUID. A file whose name starts with a UUID, but is not exactly `<uuid>.json` (or `<uuid>.<ext>` in `files/`), is a conflict copy of the file with that UUID. A file whose name starts with `workspace` and ends with `.mirante`, other than `workspace.mirante`, is a conflict copy of it.

## Alternatives

- **Match each sync tool's format.** Precise, but each tool and each version needs its own pattern, and an unknown format would be loaded as a second entity with the same id.

## Consequences

- No pattern needs updating when a sync tool changes its format.
- Any other file whose name starts with a UUID is treated as a copy too, which is the safe choice: it is listed, not loaded.
