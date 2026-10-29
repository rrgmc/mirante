# image module

An image, such as a screenshot or a diagram, with an optional link and an optional memo. See [item types](../../../docs/item-types.md#image) and [decision 0021](../../../docs/decisions/0021-image-items-as-files.md).

## Item types

| Id | Name | Rendering |
|---|---|---|
| `image` | Image | native |

The image is a file in the data folder's `files/` folder. The item's configuration holds its file name, the link and the memo.

Since M3 the type declares its configuration fields, so items can be created and edited. Its view arrives in a later milestone. Until then the cell shows the core's placeholder.

## Configuration

| Key | Type | Description |
|---|---|---|
| `file` | image, required | The image's file name in the data folder's `files/` folder. |
| `link` | URL | Optional address that a click on the image opens. |
| `memo` | multi-line text | Optional plain text shown under the image. |

The type returns `file` from `pastedImageField()`, so pasting an image onto a task dashboard creates an Image item with the pasted image in `file`.

## Connection kinds

None.

## Settings

None.
