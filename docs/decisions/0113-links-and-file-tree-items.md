# 0113. Links lists and file trees join the Notes and Folder modules

- Status: Accepted
- Date: 2026-10-31

## Context

The user wants two new panels: a list of links with titles and short descriptions, and a tree of the files in a project folder.

A link can lead to a web address or to a file. A file can sit in a project folder, which each device maps to its own path ([decision 0010](0010-folder-identity.md)), or in the workspace's `files/` ([decision 0021](0021-image-items-as-files.md)).

The user suggested [djeada/Qt-Widgets](https://github.com/djeada/Qt-Widgets) for the tree. It is a collection of example programs, with no library to link and no file tree widget. Qt has `QFileSystemModel`, which reads folders on its own thread and follows changes on disk, and `QTreeView`.

## Decision

- **Links** is a new type, `notes.links`, in the `notes` module. Like a note, a list of links is text that the user writes and that is stored in the task file. It needs nothing that Notes does not already have.
- **File tree** is a new type, `folder.tree`, in the `folder` module, which already deals with one project folder and holds the `editor` device setting.
- **The tree uses Qt alone.** It adds no dependency.
- **A link has one of three kinds:** `url`, `project` or `workspace`. Workspace files follow the size limits of [decision 0104](0104-document-items.md) and the deletion rule of [decision 0030](0030-deleting-image-files.md).
- **A web link opens only `http`, `https` and `mailto`.** Other schemes could start any program registered for them, so the editor refuses them and the view does not open them.
- **Paths must stay inside their folder.** The editor refuses an absolute path or a `..` part. The views refuse them too, because the configuration syncs and can be edited by hand.
- **Links are edited in the item dialog,** through the type's own editor. The view only reads. `EditorContext` gains `folderPath`, so that Browse can start in the folder.
- **Both views are read-only towards the disk.** The tree does not rename, delete or create files.
- **Opening a file uses the system's program for it, without asking.** An executable runs, as it does when opened from the file manager. The workspace is the user's own data, and a question on every click would not protect much.
- **The tree hides `.git` and shows other files.** It does not read `.gitignore`. Hidden files show only with `showHidden`.
- **The tree reads nothing until its view first shows,** and its expanded folders are not stored.

## Alternatives

- **A new `links` module.** It would let a build leave out links and keep notes, but no one needs that, and the item tree would gain a module with one type. [Decision 0104](0104-document-items.md) kept documents out of Notes because they need Qt PDF and the type ids would read badly. Neither applies here: `notes.links` reads well and adds no dependency.
- **Editing links in the view,** as Notes edits its text. It would make adding a link quicker, but the view would need a form and a way to copy files into `files/`, which `ItemContext` does not offer. It can come later without changing the stored data.
- **Any URL scheme in a link,** such as `vscode://`. It is convenient, but a synced value could start any registered program with a single click.
- **Following `.gitignore`.** The tree would need `git check-ignore` or its own reader of ignore rules, and a way to show the files it hides. Large folders such as `build/` cost little, since a folder is read only when it is expanded.
- **Storing which folders are expanded.** Paths differ per device, so this would be a device setting for each item. It can come later if it is missed.

## Consequences

- `EditorContext` has one more member, for any editor that needs a folder's local path.
- A link to a workspace file keeps the file in `files/`. The existing check finds the name inside the `links` array, because it reads strings at any depth.
- Each visible file tree holds a `QFileSystemModel`, with its own thread and folder watches, while its view exists. It stays while the view is hidden, so that the expanded folders survive switching tabs.
- `QFileSystemModel` runs on its own thread, inside Qt. Module code still runs on the main thread, as [the rules for modules](../modules.md#rules-for-module-code) ask.
