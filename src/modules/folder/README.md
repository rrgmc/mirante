# folder module

One of the project's folders, with its Git state and shortcuts to a terminal and an editor, as an item. See [item types](../../../docs/item-types.md#folder--terminal).

## Item types

| Id | Name | Rendering | Description |
|---|---|---|---|
| `folder` | Folder / terminal | native | One of the project's folders: its local path, Git branch and status, with shortcuts to a terminal, an editor and the file manager. |

The module calls itself "Folder" and describes itself as: "A project's folder as an item, with its Git state and shortcuts to a terminal and an editor." Help > Modules shows the names and descriptions. See [decision 0056](../../../docs/decisions/0056-modules-window.md).

Since M3 the type declares its configuration fields, so items can be created and edited. Its view arrives in a later milestone. Until then the cell shows the core's placeholder.

## Configuration

| Key | Type | Description |
|---|---|---|
| `folder` | folder, required | The id of one of the project's folders. |

## Settings

None in M2.
