# folder module

One of the project's folders as an item: its Git state and shortcuts to a terminal and an editor, or its files as a tree. See [Folder / terminal](../../../docs/item-types.md#folder--terminal) and [File tree](../../../docs/item-types.md#file-tree) in the item types.

## Item types

| Id | Name | Rendering | Description |
|---|---|---|---|
| `folder` | Folder / terminal | native | One of the project's folders: its local path, Git branch and status, with shortcuts to a terminal, an editor and the file manager. |
| `folder.tree` | File tree | native | The files under one of the project's folders, as a tree. It follows changes on disk and opens files in their programs or in an editor. |

The module calls itself "Folder" and describes itself as: "A project's folder as an item, with its Git state, its file tree and shortcuts to a terminal and an editor." Help > Modules shows the names and descriptions. See [decision 0056](../../../docs/decisions/0056-modules-window.md).

Its UUID is `f28b2ac0-3992-4a4c-8e77-40b891218bbc`. It is at version 0.1.0, by Rangel Reale. Help > Modules also shows these, and the module's homepage. See [decision 0089](../../../docs/decisions/0089-module-identity.md).

Since M3 the type declares its configuration fields, so items can be created and edited. Since M7 it has a native view; see [View](#view).

## View

`FolderView.cpp` draws the item with the view kit's `SummaryView`:

- the title is the folder's name, the last part of its local path, since module views cannot read the project's folders;
- the lines under it are the path, the branch or "Detached at <commit>", and one line of Git status such as "2 ahead · 3 changed · 1 untracked" (also staged, behind and conflicts, without the counts that are zero), or "Clean";
- `NotARepository` shows "Not a Git repository", `GitMissing` "Git was not found", and `Failed` its message. Without Git the branch comes from `gitHead()`, which reads the files. Before the first answer it does too.

The view asks `ItemContext::gitStatus()` when it is shown and every 10 seconds while it is visible, one read at a time. The timer stops while the view is hidden. An answer equal to the last draws nothing.

**Actions:**

- **Terminal** calls `launchTerminal()` in the path with no program.
- **Editor** calls `startEditor()`, in `StartEditor.cpp`. It splits the `editor` device setting with `QProcess::splitCommand()` (`code` when empty), and calls `startProgram()` with its first part as the program, then the other parts, then the path.
- **File manager** calls `showFolder()`.

A failure shows its message on the action row's error line. A folder with no local path on this device shows "Folder not set on this device" and only **Choose folder…**, which calls `mapFolder()`. The core does not make the view again after the picker, so the view reads the path again and redraws itself.

The tests are in `tests/tst_views.cpp`, with a fake context that records the programs and answers the Git reads by hand. No program starts.

## File tree

`FileTreeView.cpp` shows a `QFileSystemModel` in a `QTreeView`, through a small proxy model that hides `.git` below the root and sorts folders first. The model is made when the view first shows. A change to the configuration makes the view again, as for any type without `viewKeys()`. See [decision 0113](../../../docs/decisions/0113-links-and-file-tree-items.md).

The tests are in `tests/tst_file_tree.cpp`, on a temporary folder, with a fake context that records the programs and a handler that records the files opened.

## Configuration

Folder / terminal:

| Key | Type | Description |
|---|---|---|
| `folder` | folder, required | The id of one of the project's folders. |

File tree:

| Key | Type | Description |
|---|---|---|
| `folder` | folder, required | The id of one of the project's folders. |
| `path` | text | A subfolder to show, relative to the folder. Empty shows the whole folder. |
| `showHidden` | boolean | Show the files the system marks as hidden. Off by default. |

## Settings

No settings screen. One device setting, kept on each device ([decision 0079](../../../docs/decisions/0079-module-device-settings.md)):

| Key | Default | Description |
|---|---|---|
| `editor` | `code` | The editor command. The folder's path is its last argument. |
