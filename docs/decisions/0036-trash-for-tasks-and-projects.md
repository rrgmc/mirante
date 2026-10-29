# 0036. Deleted tasks and projects go to a trash

- Status: Accepted
- Date: 2026-10-29

## Context

Deleting a task or a project removed it and its file at once, after a confirmation. A mistake could not be undone. The user asked for a trash: deleting moves an entry there, and from there it can be restored, or the trash emptied. Holding Shift while deleting should skip the trash.

## Decision

- Tasks and projects get an optional field, `trashedAt`: the time they went to the trash, as an ISO 8601 UTC string. Without it, or with an empty value, the entry is not in the trash. The field is kept in the entity's file, so the trash syncs between devices like any other change. Old files load as before, and `schemaVersion` does not change.
- "Delete project…" and "Delete task…" set `trashedAt` without asking, since it can be undone. With Shift held when the command runs, they delete for good after the usual confirmation. Deleting a group is not changed: it is permanent and confirmed.
- A project in the trash takes its tasks with it. Their own `trashedAt` is not set, so restoring the project brings back exactly the tasks it had.
- Entries in the trash are left out of the sidebar's other lists, Focus, the quick switcher and the project and task pickers. Pages of a task in the trash close.
- They still exist in the model, so dashboard references to them are kept. Their cells read "In trash", and a restore brings the content back.
- A fourth sidebar tab, "Trash", lists them under their groups, with "Restore" and "Delete permanently…" in the context menu, and an "Empty Trash…" button. Restoring a task whose project is in the trash restores the project too.
- Deleting for good uses the model's existing removers, so the Store deletes the files and the image files nothing refers to any more.

## Alternatives

- **A trash folder in the data folder.** Moving the file to `trash/` would keep the entity out of the model. Dashboard references would show "Removed" and be lost at the next normalisation, and a sync tool would see a delete and an add instead of a change. A field is simpler and keeps references.
- **Mark the tasks of a project in the trash too.** Restoring the project would then have to tell which tasks went with it and which were in the trash before. Leaving their field alone avoids that.
- **Undo instead of a trash.** An undo stack lives on one device and is lost when the app closes. A trash survives restarts and syncs.
- **Expire the trash after some days.** Not needed now. The user empties it by hand.

## Consequences

- Everything that lists tasks or projects must leave out entries in the trash. `Model::taskInTrash()` tells whether a task is in the trash, on its own or through its project, and `SortFilter` has a `trash` mode for the sidebar.
- Files and images of entries in the trash stay until the trash is emptied, so the data folder does not shrink when something is deleted.
- An older version of the app keeps `trashedAt` as an unknown key and shows the entries as if they were not in the trash.
- Item cells of a root dashboard read "In trash" when the dashboard is next built, such as when its tab opens; task cells follow at once.
