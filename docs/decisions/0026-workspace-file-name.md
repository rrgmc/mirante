# 0026. The workspace file is `workspace.mirante`

- Status: Superseded by [0027](0027-workspace-needs-its-file.md)
- Date: 2026-10-28

## Context

[Decision 0025](0025-workspace-file.md) put the workspace's name in `workspace.json` at the top of the data folder. A `.json` file says nothing about which app it belongs to, and the user suggested `workspace.mirante` instead.

## Decision

- The workspace file is `workspace.mirante`. Its content is unchanged: JSON with `schemaVersion`, `name`, and any keys this version does not know.
- Everything else in decision 0025 stands: the file is optional, the folder's name is shown without it, renaming writes it at once, and it is the only file M2 writes.

## Alternatives

- **Keep `workspace.json`.** It works, but does not mark the folder as a Mirante workspace.
- **A hidden file, such as `.mirante`.** Hidden files are easy to miss when copying a folder, and some sync tools skip them.

## Consequences

- A folder is recognisable as a Mirante workspace from its file list.
- The extension can later be registered with the operating system, so that opening the file opens the workspace. The packaging milestone, M10, can do that.
- A data folder written by an earlier M2 build has `workspace.json`; its name is not read. No such folder exists outside development.
