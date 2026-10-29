# 0030. Image files are deleted only when this device drops the last reference

- Status: Accepted
- Date: 2026-10-28

## Context

[Decision 0021](0021-image-items-as-files.md) keeps images in `files/` and says that a file no item refers to is deleted. A sync tool does not deliver files in order. Another device may paste an image, and the image may arrive here before the task file that refers to it. A cleanup that deletes every unreferenced file would then delete the new image.

## Decision

- When an edit on this device removes a reference to a file, for example by replacing an image or deleting an item, task, project or group, the Store deletes the file if no item refers to it any more.
- A reference is any string in any item's configuration equal to the file name. This works for items of modules that are not in the build.
- The app never deletes a file only because it finds it unreferenced on disk.

## Alternatives

- **Delete every unreferenced file on load.** Simple, but it loses images that arrive before their task file.
- **Delete unreferenced files older than a grace period.** Safer, but the right period depends on the sync tool, and a device that was off for a week would still lose files.

## Consequences

- Files left behind by a crash, or by a device that deleted an item while another device still had it, stay in `files/`. They are small in number and harmless. A cleanup command can be added later if they become a problem.
