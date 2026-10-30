# M6 checks by hand

These are the checks for [milestone M6](../roadmap.md#m6-native-item-types) that tests cannot make. Run them on Windows with the release build:

```powershell
tools/build.ps1 -Preset release
tools/run.ps1 -Preset release -Data <a test workspace> -Settings <a scratch .ini file>
```

Use a test workspace with at least one group, a project and a task. You need:

- a GitLab token with the `api` scope, and a scratch project where you may approve, comment, retry and close;
- a merge request in that project whose latest pipeline failed, and one with a running pipeline;
- a second GitLab token with only the `read_api` scope;
- a Jira Cloud API token, and a scratch issue whose status you may change and where you may comment;
- a Jira Data Center site, if one is at hand.

Tick each box, or note what is wrong next to it.

The automatic tests use responses written by hand, so nothing below has been run against a real server yet. Boxes marked **Server** check a guess about what the server answers.

## Look

Try each in a wide cell and in a cell about 250 pixels wide, in the light and the dark theme.

- [ ] GitLab MR (summary): the title, state, branches and buttons come first, then Pipeline, Approvals, People and Labels. Nothing is cut off in the narrow cell; the content wraps and scrolls.
- [ ] The status pills have readable colours inside a summary and in a table, in both themes.
- [ ] GitLab Issue (summary), GitLab CI Pipeline and Jira Issue (summary) look like the same family.
- [ ] GitLab MR List, GitLab Issue List and Jira List: the title column stays in a narrow cell, and other columns hide in turn. Clicking a column sorts by it.
- [ ] A row click shows the preview under the splitter, at a useful height. The splitter can be dragged. The close button hides the preview, and a click on the same row brings it back.
- [ ] Each "Show the…" switch in the item dialog hides its section. An item made before M6, without those keys, shows every section.

## GitLab data

- [ ] An MR summary shows the right people, labels, pipeline and approvals. With a failed pipeline, the failed jobs are listed, each with its duration and a log link.
- [ ] A CI Pipeline item shows its stages in the pipeline's order, with their jobs.
- [ ] **Server.** The MR List's Pipeline and Approvals columns fill for the first 25 rows, and later rows show a dash. Turning both off stops those requests.
- [ ] **Server.** "Review requested from me" lists the merge requests that wait for your review, in the project, group and all scopes.
- [ ] **Server.** A custom filter such as `labels=bug` narrows the list, on both list types.
- [ ] **Server.** An issue with many system notes still shows its latest comments. The request asks for 20 notes and system notes are left out afterwards, so a very busy issue could show few.

## GitLab actions

Use the scratch project.

- [ ] Approve: the button goes away, and the approvals update without pressing refresh.
- [ ] Retry pipeline on a failed pipeline, and Retry on one failed job.
- [ ] **Server.** Cancel pipeline asks first, then cancels. The button shows in the states GitLab can cancel, such as running and pending, and not in others.
- [ ] Comment on a merge request and on an issue. Empty text cannot be sent.
- [ ] Close an issue, then Reopen it.
- [ ] With the `read_api` token, an action shows that the token cannot do this and names the `api` scope. The server keeps working, and the cell shows no "Token refused".

## Jira

- [ ] A Jira summary shows the status, type, priority, people, sprint, description, comments, sub-tasks and links.
- [ ] **Server.** The sprint is right. It is found by the field's name, "Sprint".
- [ ] **Server.** On an issue with a long comment thread, the comments shown are the latest five. If Jira returns only the oldest comments with the issue, this needs a request of its own.
- [ ] A description with an image shows no remote image, and nothing is fetched for it. A link in a description opens in the external browser.
- [ ] Change status: the menu lists what Jira allows now, and choosing one moves the issue. The "Status changed" badge does not stay after you look at it.
- [ ] Comment on an issue.
- [ ] Jira List: each preset, and a JQL query. Without a project key, the two project presets say to set it.
- [ ] A Jira Data Center site, if one is at hand; otherwise note that it was not checked.

## Pages and links

- [ ] "Open page" shows the page in the same cell. "Back to summary" returns to the same view, with its scroll position.
- [ ] `Shift` with "Open page" opens the external browser.
- [ ] "Web" opens the external browser, on every native view and list. It is hidden on a branch without pipelines.
- [ ] **Server.** "Web" on a list opens the matching page: a project's list, a group's list, the dashboard, and a Jira search.
- [ ] Open several pages until the memory limit in the settings window is passed. A page that is unloaded goes back to its summary, without a Reload placeholder.

## Row menu

- [ ] "Add to task as item" adds the item to the same task, in the sidebar, with a good name. No dashboard changes.
- [ ] "Copy link" and "Open page" from the row menu.

## Errors

- [ ] No token: the cell shows "Token not set", and the view says why instead of "Loading…" forever.
- [ ] Unplug the network: the views keep their data, and the cell shows its age.
- [ ] Links pasted into the item dialog still fill the types as before.
