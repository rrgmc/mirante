# notes module

Markdown notes, stored in the task file, as an item. See [item types](../../../docs/item-types.md#notes).

## Item types

| Id | Name | Rendering | Description |
|---|---|---|---|
| `notes` | Notes | native | A Markdown note with a preview. Its text is kept in the task's file. |

The module calls itself "Notes" and describes itself as: "Markdown notes, kept in the task's file, so they sync with the workspace." Help > Modules shows the names and descriptions. See [decision 0056](../../../docs/decisions/0056-modules-window.md).

The type has an id, a name and an icon, and declares that it is native. Its view arrives in a later milestone. Until then the cell shows the core's placeholder.

## Configuration

None. The type declares no fields, so the item dialog edits only the name. The note's text is edited in the view.

## Settings

None in M2.
