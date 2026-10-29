# 0051. Pasted images become items of a task, not cells of a dashboard

- Status: Accepted
- Date: 2026-10-29
- Supersedes: [0032](0032-paste-images-into-root-dashboards.md), and the parts of [0045](0045-preview-and-crop-pasted-images.md) about pasting onto dashboards

## Context

`Ctrl+V` with an image on a dashboard made a new Image item and put it in a cell ([decision 0021](0021-image-items-as-files.md)). On a root dashboard, a second dialog asked for the item's task ([decision 0032](0032-paste-images-into-root-dashboards.md)). The user found that this put images on dashboards they did not mean to change. An image belongs to a task first. Where it shows is a separate choice. The user asked that images are pasted only from a task's Items menu, and never added straight to a dashboard.

## Decision

- The context menu of a task's Items row, and of the task row, has **Paste Image…**, after "New item…". It is enabled only while the clipboard holds an image.
- It opens the paste dialog with the preview and crop of [decision 0045](0045-preview-and-crop-pasted-images.md). "Paste" adds the cropped image to that task as a new Image item, named "Image", "Image 2" and so on. No dashboard changes. The item shows in the sidebar, and the user drags it onto a dashboard if wanted.
- A paste that cannot happen shows its reason in the status bar, without a dialog. This covers the read-only sample data and a build without an image type.
- `Ctrl+V` no longer adds items. It still replaces the image of the Image item in the focused cell, or in the current item tab, after the paste dialog. This changes an item, not a dashboard, so it works on a locked dashboard too. Anywhere else, the status bar says "To add an image, use Paste Image… in a task's Items menu."
- The task question of decision 0032 goes away, since the menu names the task.
- The "Paste" button of an image field in the item dialog stays as it is.

## Alternatives

- **`Ctrl+V` does nothing at all.** Simpler, but replacing the image of a focused Image item is quick and changes no dashboard.
- **`Ctrl+V` on a sidebar task adds an item.** Faster than a menu, but the sidebar takes `Ctrl+V` for nothing else, and it is easy to press with the wrong row selected.
- **Keep pasting onto dashboards, and add the menu entry.** The user asked for one way, so a dashboard never changes by accident.

## Consequences

- Putting a screenshot on a dashboard takes two steps: Paste Image…, then a drag from the sidebar.
- `ImagePaste` no longer places items in layouts, and no longer needs the lock state.
- A pasted image always writes one file: the task's.
