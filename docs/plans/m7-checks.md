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

## Quick add

- [ ] File > Quick Add…, `Ctrl+Shift+N` and the "Quick Add" button in the sidebar header open the dialog. A one-line link on the clipboard fills the text box.
- [ ] Pasting a merge request link shows "Reading…", then its title, the project that matches `gitlab.project`, and two items: the MR summary and its CI Pipeline. A Jira key in its title or branch adds a Jira Issue item under "Related".
- [ ] Pasting a Jira key, and a Jira issue link, shows the summary as the title, the project that matches `jira.projectKey`, and a Jira Issue item. Merge requests whose branch or title hold the key appear under "Related".
- [ ] With no matching project, the dialog says "No project matches …" and Create waits for a choice. With two matching projects, it says "Several projects match". A new project choice searches again.
- [ ] Text that no module reads says "Mirante does not recognise this text."
- [ ] Changing the Profile reads the text again with that profile's servers.
- [ ] Unchecking an item and renaming the title, then Create, makes the task with only the checked items, opens it in the preview slot, and shows it in the sidebar. With several items, its dashboard shows them as tabs in one cell; with one, the item fills the cell.
- [ ] Cancel while "Reading…" closes the dialog, and nothing is created.

## Removed Claude session (local)

- [ ] A workspace that still has a `claude.session.local` item, such as one from before [decision 0084](../decisions/0084-no-local-claude-sessions.md), loads. Its cell says "This item type is no longer available: claude.session.local." Its dialog changes only the name, and its task file keeps the item unchanged after a save.
- [ ] Settings > Terminal has no device name. Settings > Modules shows only the folder's editor command.

## Folder / terminal

- [ ] In a repository: the path, the branch, and a status line such as "2 ahead · 3 changed · 1 untracked", or "Clean". Changing a file in an editor updates the line within about 10 seconds.
- [ ] In a worktree: its own branch. On a detached head: "Detached at <commit>". In a plain folder: "Not a Git repository", and the buttons still work.
- [ ] Terminal opens a terminal in the folder. Editor opens it with `code`, or the command in Settings. File manager opens it in Explorer.
- [ ] With `wt.exe` not found (rename it, or set a terminal command that does not exist and clear it again), Terminal opens the `cmd.exe` fallback in the folder. A folder path with spaces and a `&` in it works.
- [ ] A terminal command template in Settings, such as `wt.exe -p "Windows PowerShell" -d {dir} {command}`, is used instead of the default.
- [ ] An editor command that does not exist shows "<program> was not found."

## CI Pipeline following the main folder

- [ ] A CI Pipeline item with "current branch of the main folder" shows the pipelines of the main folder's branch.
- [ ] Switching branches in a terminal while the item shows updates it within a few seconds, without waiting for the poll interval.
- [ ] On a detached head it says "The main folder is not on a branch." With the main folder not mapped on this device, it says "Set the main folder's path on this device."

## Notes

- [ ] A new Notes item opens in Edit. Typing and waiting a second writes the task file; the text survives a restart.
- [ ] Preview shows the Markdown. A link opens in the browser. An image from the web is not loaded.
- [ ] `Ctrl+E` switches between Edit and Preview in the focused note only.
- [ ] A second app instance on the same workspace shows the change. Editing the note there while the first instance has no unsaved text updates the first.
- [ ] The cursor and the undo history stay while typing, as saving does not remake the editor.

## Image

- [ ] An Image item shows the image scaled to fit, with its proportions kept, in a wide and a narrow cell. The memo shows under it.
- [ ] With a link, the pointer is a hand over the image and a click opens the link.
- [ ] `Ctrl+V` with an image on the clipboard, in the focused Image cell, replaces the image.

## Builds

- [ ] The `release` and `ci` presets build on Windows.
- [ ] CI on Linux passes after the push.
- [ ] macOS, at the next manual build: Resume in terminal opens Terminal.app in the folder. CI does not check macOS.
