# image module

An image, such as a screenshot or a diagram, with an optional link and an optional memo. See [item types](../../../docs/item-types.md#image) and [decision 0021](../../../docs/decisions/0021-image-items-as-files.md).

## Item types

| Id | Name | Rendering |
|---|---|---|
| `image` | Image | native |

The image is a file in the data folder's `files/` folder. The item's configuration holds its file name, the link and the memo.

In M2 the type is a stub with an id, a name and an icon. Pasting from the clipboard arrives in M3, and the view in a later milestone.

## Connection kinds

None.

## Settings

None.
