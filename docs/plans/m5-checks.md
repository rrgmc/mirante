# M5 checks by hand

These are the checks for [milestone M5](../roadmap.md#m5-servers-and-status) that tests cannot make. Run them on Windows with the release build:

```powershell
tools/build.ps1 -Preset release
tools/run.ps1 -Preset release -Data <a test workspace> -Settings <a scratch .ini file>
```

The sections on tokens and on the workspace id test the keychain itself, so run them without `-NoKeychain`. Tokens then go to Windows Credential Manager, as generic credentials whose names start with `mirante/`. Every other section can add `-NoKeychain`, so a test run leaves this device's keychain alone; type the tokens again after each start in that case.

Use a test workspace with at least one group, a project and a task. You need:

- a GitLab personal access token for gitlab.com, with the `read_api` scope, and a merge request whose latest pipeline failed;
- a Jira Cloud API token for your Atlassian site, and an issue whose status you may change;
- a Jira Data Center site, if one is at hand.

Tick each box, or note what is wrong next to it.

## Tokens and the keychain

- [X] Profiles > the profile > Modules > GitLab: set the server to `https://gitlab.com` and type the token. After OK, open the screen again. The token field is empty, its placeholder says "Set on this device. Type to replace it.", and it has a Remove button.
- [X] The same for Jira, with your Atlassian site, the deployment left at "auto", your email and the API token.
- [X] Windows Credential Manager, Generic Credentials, has two entries for `mirante`, named `mirante/<workspace id>/<profile id>/gitlab/token` and `mirante/<workspace id>/<profile id>/jira/token`. The workspace id is the `id` in `workspace.mirante`.
- [ ] Quit and start again. Both screens still say the token is set, and the items below show their status without typing anything.
- [ ] Remove on the GitLab screen, then OK: its entry is gone from Credential Manager, and the screen says "Not set on this device.". Type the token again afterwards.
- [ ] Make a second profile, set a GitLab token in it, then delete the profile. The question mentions its tokens. Its entry is gone from Credential Manager.
- [ ] Quit. Change the `id` in `workspace.mirante` by hand, as another device would. Start again. The Credential Manager entries now use the new id, the old ones are gone, and the GitLab item still shows its status.

## Project settings

- [ ] The project dialog has a GitLab section with "GitLab project path" and a Jira section with "Jira project key", after the folders. Values typed there are kept after OK, and emptying one removes it from the project's file.

## Status in cells

- [ ] A GitLab MR (page) item of the merge request whose pipeline failed shows a red badge in its cell header. Its tooltip names the reason. The task, project and group in the sidebar, and the task in Focus, show a badge too.
- [ ] A Jira Issue (page) item. Change the issue's status in Jira. After the next fetch (or the refresh button), the cell shows a badge for "Status changed to …". It clears after the cell has been visible for two seconds in the active window. It does not clear while another window is active.
- [ ] The refresh button in a cell with status fetches at once. Cells of types without status, such as a Web page, have no refresh button.
- [ ] Tools > Settings…, Status: change "Check servers every" to 1 minute. Status changes made on the server show within about a minute.
- [ ] Focus: "Needs attention first" puts the tasks with a badge at the top. The choice is kept after a restart.

## Problems

- [ ] Type a wrong GitLab token. The cell shows "Token refused", and the warning bar has a line for GitLab in that profile, with a "Set Token…" button. Polling stops: the log, or the server's activity, shows no new requests. Typing the right token removes the line, and status comes back.
- [ ] Remove the Jira token. The Jira cell shows "Token not set". Its button opens the Jira screen of the right profile.
- [ ] Unplug the network, or turn off Wi-Fi, and wait for a fetch. The badges stay, and the cells show the age of their data, such as "5 min ago". Plug it back in; the age goes.
- [ ] A Jira Data Center site with a personal access token shows its issues' status. If no site is at hand, write "not checked" here.
