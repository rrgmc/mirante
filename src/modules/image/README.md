# image module

An image, such as a screenshot or a diagram, with an optional link and an optional memo. See [item types](../../../docs/item-types.md#image) and [decision 0021](../../../docs/decisions/0021-image-items-as-files.md).

## Item types

| Id | Name | Rendering | Description |
|---|---|---|---|
| `image` | Image | native | An image, such as a screenshot, scaled to fit the cell, with an optional memo under it. A click opens its link, if it has one. |

The module calls itself "Image" and describes itself as: "Images, such as screenshots or diagrams, with an optional link and memo." Help > Modules shows the names and descriptions. See [decision 0056](../../../docs/decisions/0056-modules-window.md).

The image is a file in the data folder's `files/` folder. The item's configuration holds its file name, the link and the memo.

Since M3 the type declares its configuration fields, so items can be created and edited. Its view arrives in a later milestone. Until then the cell shows the core's placeholder.

## Configuration

| Key | Type | Description |
|---|---|---|
| `file` | image, required | The image's file name in the data folder's `files/` folder. |
| `link` | URL | Optional address that a click on the image opens. |
| `memo` | multi-line text | Optional plain text shown under the image. |

The type returns `file` from `pastedImageField()`, so "Paste Image…" in a task's Items menu creates an Image item with the pasted image in `file`, and `Ctrl+V` on a focused Image item replaces it.

## Settings

None.
