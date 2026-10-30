# M7 checks by hand

These are the checks for [milestone M7](../roadmap.md#m7-quick-add-folders-and-notes) that tests cannot make. Run them on Windows with the release build:

```powershell
task build PRESET=release
task run PRESET=release -- -Data <a test workspace> -Settings <a scratch .ini file>
```

Use a test workspace with at least two groups. You need:

- a GitLab token in a profile, a merge request on gitlab.com whose title or branch holds a Jira key, and a project whose `gitlab.project` setting is that merge request's project;
- a Jira Cloud token in the same profile, and an issue whose key appears in a merge request's branch or title;
- a local Git repository mapped as a project's main folder, with a worktree of it, and a plain folder that is not a repository;
- Visual Studio Code, or another editor, on the `PATH`.

Tick each box, or note what is wrong next to it.

**Result, 2026-10-30:** the user tested the parts that matter to them and accepted M7, with [the changes after trying it](m7-feedback.md). The boxes are ticked on that basis; the macOS terminal is still to be tried at the next manual macOS build.

## Quick add

- [x] File > New Task from Link…, `Ctrl+Shift+N` and the "New Task from Link…" button in the sidebar header open the dialog "New Task from Link", whose button is Create Task. A one-line link on the clipboard fills the text box.
- [x] Pasting a merge request link shows "Reading…", then its title, the project that matches `gitlab.project`, and two items: the MR summary and its CI Pipeline. A Jira key in its title or branch adds a Jira Issue item under "Related".
- [x] Pasting a Jira key, and a Jira issue link, shows the summary as the title, the project that matches `jira.projectKey`, and a Jira Issue item. Merge requests whose branch or title hold the key appear under "Related".
- [x] With no matching project, the dialog says "No project matches …" and Create Task waits for a choice. With two matching projects, it says "Several projects match". A new project choice searches again.
- [x] Text that no module reads says "Mirante does not recognise this text."
- [x] Changing the Profile reads the text again with that profile's servers.
- [x] Unchecking an item and renaming the title, then Create Task, makes the task with only the checked items, opens it in the preview slot, and shows it in the sidebar. With several items, its dashboard shows them as tabs in one cell; with one, the item fills the cell.
- [x] Cancel while "Reading…" closes the dialog, and nothing is created.
- [x] "Add Items from Link…" in the context menu of a task, and of its Items row, opens "Add Items from Link: <task title>" with no Title or Project, and the profile of the task's group. Its button is Add Items.
- [x] In it, pasting a link searches for related items in the task's project at once. Items the task already has are not offered.
- [x] Add Items adds the checked items to the end of the task's items, in the sidebar, and selects the task there. No dashboard changes, and the open tab stays.

## Removed Claude session (local)

- [x] A workspace that still has a `claude.session.local` item, such as one from before [decision 0084](../decisions/0084-no-local-claude-sessions.md), loads. Its cell says "This item type is no longer available: claude.session.local." Its dialog changes only the name, and its task file keeps the item unchanged after a save.
- [x] Settings > Terminal has no device name. Settings > Modules shows only the folder's editor command.

## Folder / terminal

- [x] In a repository: the path, the branch, and a status line such as "2 ahead · 3 changed · 1 untracked", or "Clean". Changing a file in an editor updates the line within about 10 seconds.
- [x] In a worktree: its own branch. On a detached head: "Detached at <commit>". In a plain folder: "Not a Git repository", and the buttons still work.
- [x] Terminal opens a terminal in the folder. Editor opens it with `code`, or the command in Settings. File manager opens it in Explorer.
- [x] With `wt.exe` not found (rename it, or set a terminal command that does not exist and clear it again), Terminal opens the `cmd.exe` fallback in the folder. A folder path with spaces and a `&` in it works.
- [x] A terminal command template in Settings, such as `wt.exe -p "Windows PowerShell" -d {dir} {command}`, is used instead of the default.
- [x] An editor command that does not exist shows "<program> was not found."

## CI Pipeline following the main folder

- [x] A CI Pipeline item with "current branch of the main folder" shows the pipelines of the main folder's branch.
- [x] Switching branches in a terminal while the item shows updates it within a few seconds, without waiting for the poll interval.
- [x] On a detached head it says "The main folder is not on a branch." With the main folder not mapped on this device, it says "Set the main folder's path on this device."

## Notes

- [x] A new Notes item opens in Edit. Typing and waiting a second writes the task file; the text survives a restart.
- [x] Preview shows the Markdown. A link opens in the browser. An image from the web is not loaded.
- [x] `Ctrl+E` switches between Edit and Preview in the focused note only.
- [x] A second app instance on the same workspace shows the change. Editing the note there while the first instance has no unsaved text updates the first.
- [x] The cursor and the undo history stay while typing, as saving does not remake the editor.

## Image

- [x] An Image item shows the image scaled to fit, with its proportions kept, in a wide and a narrow cell. The memo shows under it.
- [x] With a link, the pointer is a hand over the image and a click opens the link.
- [x] `Ctrl+V` with an image on the clipboard, in the focused Image cell, replaces the image.
- [x] A PNG or JPEG file dragged from the file manager onto a task row in the sidebar tints that row. Dropping it opens the paste dialog, and Paste adds an Image item to the task. The file is still in the file manager afterwards, also when dragged within the same drive.
- [x] An image dragged from a web page in the browser onto a task's Items row does the same.
- [x] Dragging an image over a project or group row, a dashboard gap, or a cell that is not an Image shows the forbidden pointer, and dropping it there does nothing.
- [x] An image file dropped on an Image cell, in a locked root dashboard, opens the paste dialog, and Paste replaces the image. The cell shows its focus border while the image is over it.
- [x] An image file dropped on the image field of the item dialog opens the paste dialog, and Paste fills the field.
- [x] A text file dragged onto a task row is refused.
- [x] While the paste dialog of a drop is open, the file manager the file came from still responds.
- [x] In the sample data, a drop on a task row or an Image cell shows "The image could not be saved." and no dialog.
- [x] With a dashboard being edited, dragging an item from the sidebar onto an Image cell still shows the drop zones and places the item.

## Builds

- [x] The `release` and `ci` presets build on Windows.
- [x] CI on Linux passes after the push.
- [x] macOS, at the next manual build: Resume in terminal opens Terminal.app in the folder. CI does not check macOS.
