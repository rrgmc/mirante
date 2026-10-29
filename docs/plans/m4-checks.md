# M4 checks by hand

These are the checks for [milestone M4](../roadmap.md#m4-web-views) that tests cannot make. Run them on Windows:

```powershell
tools/build.ps1 -Preset release
tools/run.ps1 -Preset release -Data <a test workspace> -Settings <a scratch .ini file>
```

Use the release build. The debug build loads Qt WebEngine's debug libraries, whose extra internal checks stop some pages, such as the claude.ai login page.

Use a test workspace with at least two groups, A and B, each with a project and a task. Logins are kept in the app's local data folder, not in the workspace, so they are real logins on this device. The scratch settings file keeps your own last workspace and open tabs unchanged.

Tick each box, or note what is wrong next to it.

## Logins

Do these first. They test the [known risk](../web-views.md#known-risk-logins-in-embedded-browsers). Note for each one how you signed in (password, email code, Google, company sign-in) and whether it worked.

- [X] In group A, a Claude session (web) item with a claude.ai URL. Sign in to claude.ai inside the cell, or in a maximised cell. (Google account, work)
- [X] In group A, a GitLab connection to `https://gitlab.com` (group dialog, Connections) and a GitLab MR (page) item. Sign in to gitlab.com. The merge request page shows. (Google account, work)
- [X] In group A, a Jira Cloud connection to your Atlassian site and a Jira Issue (page) item. Sign in. The issue page shows. (Google account, work)
- [X] A login popup, such as "Sign in with Google", opens in a dialog of at least 500 × 600 that can be resized, and closes by itself when the sign-in is done.

  A dialog opens only when the site opens its sign-in in a popup window. Most Google sign-ins, such as those of GitLab and Atlassian, load in the same page instead. Two ways to get one:

  1. Sign out of claude.ai in the Claude session (web) item, then choose "Continue with Google". The Google sign-in opens in the dialog. It closes by itself when the sign-in is done.
  2. A Web page item with this URL, which checks the size and the closing without a login:

     ```
     data:text/html,<button onclick="w=window.open('','login','width=300,height=200');w.document.write('<button onclick=window.close()>Done</button>')">Open popup</button>
     ```

     "Open popup" opens a dialog of 500 × 600, although the page asks for 300 × 200. The dialog can be resized. "Done" closes it.
- [X] Quit and start again. All three items are still signed in.

## Profiles

- [ ] The same claude.ai URL in a Claude session (web) item in group B asks for a login: group A's login is not visible in group B.
- [ ] A Web page item in group A with a claude.ai URL also asks for a login: the `web` and `claude` modules have separate profiles ([decision 0040](../decisions/0040-profiles-per-group-and-module.md)).
- [ ] Add your Atlassian host to "Sites in the shared profile" in group A and group B. After a login in a Jira Issue (page) item of group A, the same kind of item in group B is signed in too.
- [ ] Delete group B. Its folders `g-<groupId>-*` under the app's local data folder `profiles/` are gone after a few seconds. Group A's are still there.

## Views

- [ ] The same web item in two visible cells shows two live pages. Scrolling one does not scroll the other.
- [ ] A web cell header shows back, reload, the address and "Open in browser". Back is disabled on a fresh page. The address is elided in the middle, and shows in full when clicked. The name and type pills stay readable next to the toolbar in a narrow cell.
- [ ] A link that opens a new tab, such as one with `target="_blank"`, opens in the system browser.
- [ ] An address that does not load, such as `https://nonexistent.invalid`, shows the error with a Reload button.
- [ ] A site asking to show notifications shows a bar above the page with Allow and Deny. After an answer it does not ask again, also after a restart.
- [ ] A hidden tab does not load its pages until it is opened.
- [ ] A GitLab MR (page) item in a group without a GitLab connection says "No GitLab connection in this group."

## Memory policy

- [ ] Quit, then set `hiddenLimit=1` under `[webViews]` in the scratch settings file, and start again. Open three root dashboard tabs with web items and switch between them. Going back to the tab hidden longest reloads its page; the most recently hidden one is still loaded.

## A bigger view

- [ ] The maximise button in a cell header maximises the cell, and its icon and tooltip change to Restore. A web page keeps its state, such as the scroll position and a half-typed login.
- [ ] The cell menu offers Maximise (or Restore) and Open Alone, also on a locked dashboard and on an item cell inside a task cell of a root dashboard.
- [ ] Open Alone opens the item in the preview tab, with a page of its own.

## Detached windows

- [ ] The root tab menu offers "Detach into Window". The window shows the dashboard, with its name as the title, and a lock and "Put Back" in its toolbar. Web pages keep their state when they move.
- [ ] Unlocking the detached window allows editing its layout there.
- [ ] Opening the detached dashboard from the sidebar, the switcher or the dashboard list raises its window.
- [ ] Closing the window, or "Put Back", returns the tab to the end of the tab bar.
- [ ] Quit with a dashboard detached and start again: it opens in its window, at the same place and size.
- [ ] Task tabs, item tabs, the preview tab and Focus cannot be detached.
