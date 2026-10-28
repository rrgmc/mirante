# 0025. A workspace file holds the workspace's name

- Status: Superseded by 0026
- Date: 2026-10-28

## Context

The user wants each workspace, a data folder, to have an editable name, shown above the sidebar tree. The name belongs to the workspace, not to one device, so it should travel with the synced data.

M2 reads the data folder and writes nothing; writing arrives with the Store in M3.

## Decision

- A data folder may hold `workspace.json` at its top level, next to the entity subfolders. It has `schemaVersion` and `name`, and keeps keys it does not know, like every other file.
- Without the file, or without a name in it, the app shows the folder's name. The sample data is named "Sample data".
- Renaming, from File > Rename Workspace… or a double click on the name above the tree, writes `workspace.json` at once with `QSaveFile`. This is the only file M2 writes. The rest of the data folder is still read-only until M3.
- The Store reads and writes the file; `serialise()` includes it when there is a name.

## Alternatives

- **Keep the name in the device settings.** It would differ on each device that opens the same folder.
- **Store the name in a group or another entity file.** The name belongs to the whole workspace, not to one entity.
- **Wait for M3 to make the name editable.** The user asked for it now, and writing one small file does not need M3's change tracking or conflict handling.

## Consequences

- A conflict copy of `workspace.json` from a sync tool is not detected in M2. M3's conflict handling covers it with the other files.
- The data folder layout in [sync](../sync.md#the-data-folder) gains the file.
