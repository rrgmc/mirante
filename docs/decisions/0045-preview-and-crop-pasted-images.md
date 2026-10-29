# 0045. Pasted images open a preview with a crop first

- Status: Accepted; pasting onto dashboards superseded by [0051](0051-paste-images-from-the-items-menu.md)
- Date: 2026-10-29

## Context

`Ctrl+V` with an image saved it straight away, as a new Image item or into an existing one ([decision 0021](0021-image-items-as-files.md)). The user could not see what was pasted before it was saved. A screenshot often holds more than the user wants, and cutting it down meant another tool. The user asked for a preview, with a crop rectangle that can be dragged.

## Decision

- Every image paste opens a paste dialog first. This covers `Ctrl+V` on task and root dashboards and in item tabs, and the "Paste" button of an image field in the item dialog.
- The dialog shows the image scaled to fit and its size in pixels. The user can drag a crop rectangle on it, with handles to resize it. "Whole Image" removes the crop.
- The crop is kept in the image's own pixels, so the saved image is cut at full resolution.
- "Paste" pastes the cropped image. Cancel pastes nothing.
- The paste is checked before the dialog opens. A paste that cannot happen, for example on a locked dashboard or in the read-only sample data, shows its message without a dialog. `ImagePaste::check()` answers this without saving anything.
- On a root dashboard, the task question of [decision 0032](0032-paste-images-into-root-dashboards.md) stays a separate dialog, and comes after the paste dialog.
- The crop widget, `ImageCropView`, is its own class, with its maths in plain functions, so tests can check it without a mouse.

## Alternatives

- **Paste first, crop later from the item.** Nothing is in the way of a quick paste, but the full image is saved first, and the item needs an editor it does not have.
- **A preview only for new items.** Fewer dialogs, but replacing an image is just as likely to need a crop.
- **The task question inside the paste dialog.** One dialog fewer on a root dashboard, but the dialog would differ by where the paste goes, and decision 0032's dialog would have two forms.
- **The task question first.** It would let the user cancel earlier, but the user would choose a task for an image not yet seen.

## Consequences

- Every paste takes one more step: `Ctrl+V`, then `Enter`.
- Pasted images can be smaller, so `files/` grows more slowly.
- A paste that is refused is refused without a dialog, so the extra step does not come before a refusal. Only a save that fails for another reason, such as a full disk, shows after the dialog.
