# M8 checks by hand

These are the checks for [milestone M8](../roadmap.md#m8-tray-notifications-and-keyboard) that tests cannot make. Run them on Windows with the release build:

```powershell
task build PRESET=release
task run PRESET=release -- -Data <a test workspace> -Settings <a scratch .ini file>
```

You need a GitLab token in a profile, and a project with a pipeline you can make fail, such as a branch with a job that runs `exit 1`. Add a CI Pipeline item for that branch to a task, and a Web page item for claude.ai or GitLab.

Tick each box, or note what is wrong next to it.

**Result, 2026-10-30:** the user ran the checks with the release build and accepted M8. Every box is ticked.

## Watched items

- [x] The item dialog of the CI Pipeline item has the row "Watch: Count on the tray icon, and notify", unchecked. A Web page item has no such row.
- [x] "Watch" in the item's context menu in the sidebar, and in its cell menu, is checked after ticking the box, and unticking it there clears the box.
- [x] A second instance on the same folder, with `--web-data <another folder>`, sees the change.

## Tray count

- [x] With the pipeline failed and the item not watched, the tray icon is plain. Watching it adds a red badge with "1" at the icon's lower right within 15 seconds, and the tooltip says "Mirante: 1 task needs attention".
- [x] A second watched item with attention in the same task keeps "1"; one in another task makes "2".
- [x] Marking the task Done, or moving it to the trash, clears its count.
- [x] With the window hidden in the tray, the count still follows a pipeline that fails or passes.

## Notifications

- [x] With the item watched and its pipeline passing, retrying a job so that the pipeline fails sends one notification titled "Pipeline failed", with the task and the item in its text.
- [x] Clicking the notification brings Mirante to the front, from the tray too, and opens the task in the preview.
- [x] Quitting and starting Mirante again sends nothing for the same failure.
- [x] Turning Watch off and on again while the pipeline is failed sends nothing.
- [x] An item that is not watched never notifies.

## Keys in web pages

`tst_page_keys` checks the keys in a release build. By hand:

- [x] In a GitLab or Jira comment box, `Ctrl+B` makes text bold, and `Ctrl+V` pastes text. Outside the text field, `Ctrl+B` shows or hides the sidebar.
- [x] In a claude.ai cell, `Ctrl+K` opens Mirante's quick switcher. After adding `Ctrl+K` in the item's "Keys for the page", it opens claude.ai's own search instead, and the other shortcuts still work.
- [x] `Ctrl+W`, `Ctrl+Tab`, `Ctrl+Alt+Arrow`, `F2` and `F5` work with the focus in a page.

## Menus

- [x] File lists New Task from Link…, New Group…, then New Workspace…, Open Workspace…, Open Recent and Workspace, then Quit. Workspace holds Rename…, Move…, Show Folder and Reload, which work as before, and `F5` still reloads.
- [x] Help lists Modules… and Open Sample Data, then About Mirante and About Qt.
- [x] In Brazilian Portuguese the menus have no clashing access keys.

## macOS, at the next manual build

- [x] The tray count shows as the number next to the menu bar icon, in the menu bar's colours.
- [x] The first notification asks for permission; after allowing it, a click on a notification opens the task.
