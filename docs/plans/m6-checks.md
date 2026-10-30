# M6 checks by hand

These are the checks for [milestone M6](../roadmap.md#m6-native-item-types) that tests cannot make, including [the changes after trying it](m6-feedback.md). Run them on Windows with the release build:

```powershell
task build PRESET=release
task run PRESET=release -- -Data <a test workspace> -Settings <a scratch .ini file>
```

Use a test workspace with at least one group, a project and a task. You need:

- a GitLab token with the `api` scope, and a scratch project where you may run pipelines;
- a merge request in that project whose latest pipeline failed, one with a running pipeline, and if you have one, a repository that starts several pipelines at once;
- a pipeline with a manual job;
- a second GitLab token with only the `read_api` scope;
- a Jira Cloud API token, an epic with children, and the names of custom fields on your site;
- a Jira Data Center site, if one is at hand.

Tick each box, or note what is wrong next to it.

The automatic tests use responses written by hand. Boxes marked **Server** check a guess about what the server answers.

**Result, 2026-10-30:** the user checked every box and accepted M6.

## Look

Try each in a wide cell and in a cell about 250 pixels wide, in the light and the dark theme.

- [x] GitLab MR (summary): the title, state, branches and buttons come first, then Pipeline, Approvals, People and Labels. Nothing is cut off in the narrow cell; the content wraps and scrolls.
- [x] The status pills have readable colours inside a summary and in a table, in both themes.
- [x] GitLab Issue (summary), GitLab CI Pipeline and Jira Issue (summary) look like the same family.
- [x] The CI Pipeline's job table in a very narrow cell. Its links and pills do not wrap, so the view may scroll sideways; note if that is a problem.
- [x] GitLab MR List, GitLab Issue List, GitLab CI Pipeline List and Jira List: the title column and the links stay in a narrow cell, and other columns hide in turn. Clicking a column sorts by it.
- [x] Each "Show the…" switch in the item dialog hides its section. An item made before M6, without those keys, shows every section.

## Lists

- [x] Each row has "Web", "Open" and "Add". There is no preview.
- [x] `limit` and `pageSize` in the item's settings. Previous and Next step through pages, and a line such as "1-50 of 100" is right. Sorting and filtering cover all the fetched rows.
- [x] Dates are compact, such as `10-30 14:32`, and sort by the real date.
- [x] Jira List: the Status filter. Its choice survives a refresh.
- [x] **Server.** The MR List's Pipeline and Approvals columns fill for the first 25 rows of the page, and later rows show a dash. Turning both off stops those requests.
- [x] **Server.** "Review requested from me" lists the merge requests that wait for your review, in the project, group and all scopes.
- [x] **Server.** A custom filter such as `labels=bug` narrows the list, on both list types.
- [x] GitLab CI Pipeline List: each source, all, mine, a branch and a merge request. "Add" makes a pipeline item for the row's branch or merge request.
- [x] **Server.** The pipeline list's Author usually shows a dash, because GitLab's list answers leave it out. Note if the column is worth keeping.

## GitLab summaries and pipelines

- [x] No approve, comment, close or reopen button anywhere.
- [x] An MR summary shows the right people, labels, pipeline and approvals, and its Pipeline row has a "Web" link to the pipeline's page.
- [x] A CI Pipeline item: jobs in a table grouped by stage, in the pipeline's order, with colour pills, durations and links.
- [x] A link to the merge request, on an item that follows a merge request, and on a branch that has an open one.
- [x] A running pipeline refreshes every 15 seconds while its cell is visible, and stops when it finishes or the cell is hidden.
- [x] **Server.** A repository that starts several pipelines at once: they show as a row of pills, and each opens with its jobs. Is 10 minutes the right window? A failure in any of them raises the badge.
- [x] **Server.** Previous and Next step through older pipelines.
- [x] Retry pipeline, Retry on a job, and Play on a manual job.
- [x] **Server.** Cancel pipeline asks first, then cancels. The button shows in the states GitLab can cancel, such as running and pending, and not in others.
- [x] "Trigger pipeline" asks first, then starts one on the branch.
- [x] A new pipeline item has `master` as its branch, and an empty branch means `master`.
- [x] With the `read_api` token, a CI action shows that the token cannot do this and names the `api` scope. The server keeps working, and the cell shows no "Token refused".
- [x] **Server.** An issue with many system notes still shows its latest comments. The request asks for 20 notes and system notes are left out afterwards, so a very busy issue could show few.

## Jira

- [x] No change status or comment button anywhere.
- [x] A Jira summary shows the status, type, priority, people, sprint, description, comments, sub-tasks and links.
- [x] An epic shows its children with status pills, each linking to the web, up to 50, with a count of the rest.
- [x] **Server.** On Data Center, an issue that is not an epic still shows its children under `parent`.
- [x] Extra fields: add "Team" and "Fix version" to the Jira settings. They show in a summary's Details and as list columns. A name your site lacks is left out quietly.
- [x] **Server.** The sprint is right. It is found by the field's name, "Sprint".
- [x] **Server.** On an issue with a long comment thread, the comments shown are the latest five. If Jira returns only the oldest comments with the issue, this needs a request of its own.
- [x] A description with an image shows no remote image, and nothing is fetched for it. A link in a description opens in the external browser.
- [x] Jira List: each preset, and a JQL query. Without a project key, the two project presets say to set it.
- [x] A Jira Data Center site, if one is at hand; otherwise note that it was not checked.

## Pages and links

- [x] "Open page" shows the page in the same cell. "Back to summary" returns to the same view, with its scroll position.
- [x] `Shift` with "Open page" opens the external browser.
- [x] "Web" opens the external browser, on every native view and list. It is hidden on a branch without pipelines.
- [x] **Server.** "Web" above a list opens the matching page: a project's list, a group's list, the dashboard, and a Jira search.
- [x] Open several pages until the memory limit in the settings window is passed. A page that is unloaded goes back to its summary, without a Reload placeholder.

## Row menu

- [x] "Add to task as item" adds the item to the same task, in the sidebar, with a good name. No dashboard changes. A failed add shows nothing yet.
- [x] "Copy link" and "Open page" from the row menu.

## The rest of the app

- [x] Drag root dashboards and other tabs to reorder them. Focus and the preview stay fixed. The order comes back after a restart.
- [x] Drag tabs within a dashboard's tab group while it is unlocked. A locked dashboard does not change. Dragging a tab onto another cell still works.
- [x] The project dialog shows a module's section only if the project has values for it. "Add settings" adds one, and existing values are kept.
- [x] The item dialog's profile choice reads "Work (from the group)".
- [x] Change a group file's `schemaVersion` to 9999 in a copy of a workspace, then open it. It is refused, the warning bar says to update Mirante, and what was open stays open. Start the app with that as the last workspace: the window is empty and nothing is written.
- [x] `task` lists the tasks, and `task test` builds and tests.

## Errors

- [x] No token: the cell shows "Token not set", and the view says why instead of "Loading…" forever.
- [x] Unplug the network: the views keep their data, and the cell shows its age.
- [x] Links pasted into the item dialog still fill the types as before.
