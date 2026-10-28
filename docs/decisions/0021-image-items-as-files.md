# 0021. Image items, with images stored as files

- Status: Accepted
- Date: 2026-10-28

## Context

The user wants an item that shows an image, such as a screenshot or a diagram, with an optional link and an optional memo, and that can be filled by pasting from the clipboard.

Everything else in the data folder is small JSON, one file per entity ([decision 0004](0004-file-per-entity-sync.md)). An image is binary and can be large.

## Decision

- A new module, `image`, has one item type, `image` ("Image"), rendered natively.
- Its configuration holds `file`, the image's file name; `link`, an optional URL opened by a click on the image; and `memo`, optional plain text shown under the image.
- The image itself is a file in a `files/` folder of the data folder, named `<uuid>.<ext>`, such as `files/2f6c….png`. The task file refers to it by name. A pasted image is saved as PNG.
- Pasting an image from the clipboard into an Image item, or into a dashboard as a new Image item, saves the file and sets `file`. Pasting arrives with item editing in M3; the view arrives with the native item types in M6.
- A file no item refers to any more is deleted when the data folder is written. A missing file shows "Image not found" in the item.

## Alternatives

- **The image as base64 in the task file.** One file per item stays simple, but every task change rewrites the whole image, task files grow large, and sync conflicts on a task file would carry the image along.
- **The image as a link only.** It needs the image to be hosted somewhere, which clipboard pastes are not.

## Consequences

- The data folder gains a `files/` folder, which [sync](../sync.md#the-data-folder) and the Store handle: images sync like any other file, and a conflict copy of an image is a new file that nothing refers to.
- Modules need a way to store and read data-folder files. `ItemContext` gets file access when the image view is built.
