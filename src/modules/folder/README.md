# folder module

One of the project's folders, with its Git state and shortcuts to a terminal and an editor, as an item. See [item types](../../../docs/item-types.md#folder--terminal).

## Item types

| Id | Name | Rendering | Description |
|---|---|---|---|
| `folder` | Folder / terminal | native | One of the project's folders: its local path, Git branch and status, with shortcuts to a terminal, an editor and the file manager. |

The module calls itself "Folder" and describes itself as: "A project's folder as an item, with its Git state and shortcuts to a terminal and an editor." Help > Modules shows the names and descriptions. See [decision 0056](../../../docs/decisions/0056-modules-window.md).

Since M3 the type declares its configuration fields, so items can be created and edited. Since M7 it has a native view; see [View](#view).

## View

`FolderView.cpp` draws the item with the view kit's `SummaryView`:

- the title is the folder's name, the last part of its local path, since module views cannot read the project's folders;
- the lines under it are the path, the branch or "Detached at <commit>", and one line of Git status such as "2 ahead · 3 changed · 1 untracked" (also staged, behind and conflicts, without the counts that are zero), or "Clean";
- `NotARepository` shows "Not a Git repository", `GitMissing` "Git was not found", and `Failed` its message. Without Git the branch comes from `gitHead()`, which reads the files. Before the first answer it does too.

The view asks `ItemContext::gitStatus()` when it is shown and every 10 seconds while it is visible, one read at a time. The timer stops while the view is hidden. An answer equal to the last draws nothing.

**Actions:**

- **Terminal** calls `launchTerminal()` in the path with no program.
- **Editor** splits the `editor` device setting with `QProcess::splitCommand()` (`code` when empty), and calls `startProgram()` with its first part as the program, then the other parts, then the path.
- **File manager** calls `showFolder()`.

A failure shows its message on the action row's error line. A folder with no local path on this device shows "Folder not set on this device" and only **Choose folder…**, which calls `mapFolder()`. The core does not make the view again after the picker, so the view reads the path again and redraws itself.

The tests are in `tests/tst_views.cpp`, with a fake context that records the programs and answers the Git reads by hand. No program starts.

## Configuration

| Key | Type | Description |
|---|---|---|
| `folder` | folder, required | The id of one of the project's folders. |

## Settings

No settings screen. One device setting, kept on each device ([decision 0079](../../../docs/decisions/0079-module-device-settings.md)):

| Key | Default | Description |
|---|---|---|
| `editor` | `code` | The editor command. The folder's path is its last argument. |
