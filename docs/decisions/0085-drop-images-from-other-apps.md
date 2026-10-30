# 0085. Images can be dropped from other apps

- Status: Accepted
- Date: 2026-10-30

## Context

Images reach Mirante only from the clipboard: Paste Image… in a task's menu, `Ctrl+V` on an Image cell, and the Paste button of an image field ([decision 0051](0051-paste-images-from-the-items-menu.md)). After trying M7, the user wants to drag an image file from the file manager, or an image from a browser, into the app.

## Decision

A drop takes an image file (PNG, JPEG, GIF, BMP or WebP, the first one if several) or image data, and goes where the same image would go by pasting:

- **On a task, or its Items row, in the sidebar:** the [paste dialog](../ui.md#pasting-images) opens with the image, and OK adds an Image item to that task, as Paste Image… does.
- **On a cell that shows an Image item:** the paste dialog opens, and OK replaces that item's image, as `Ctrl+V` does.
- **On an image field of the item dialog:** the field takes the image, as its Paste button does.

Anywhere else the drop is refused, and the pointer says so while dragging. A file that is not an image, or that cannot be read, is refused too. The paste dialog always opens first, so the user sees and can crop what is saved.

## Alternatives

- **Dropping onto a dashboard cell to add an item there.** Decision 0051 keeps pasted images off dashboards; a drop follows the same rule.
- **Saving at once, without the dialog.** Faster, but a drop is easier to make by mistake than a paste.

## Consequences

- The sidebar, the Image view's cell and the image field accept drops of files and images, alongside the sidebar's own drags.
- Each place reuses `ImagePaste`, so the rules for the read-only sample data and failed saves stay the same.
