# Item types

An item is one panel in a task. Its type decides what the user configures, what the panel shows and what actions it offers. Fields shared by all items are in the [data model](data-model.md#item).

Every item type except built-in panels belongs to a [module](modules.md). A build includes only the enabled modules, so it may offer only some of the types below.

Summary views and web page views are separate types, for example "GitLab MR (summary)" and "GitLab MR (page)". See [decision 0007](decisions/0007-separate-item-types-per-rendering.md).

## The type registry

A module registers each of its types in the `ItemTypeRegistry` with:

| Property | Description |
|---|---|
| `id` | Stable string stored in task files, such as `gitlab.mr.summary`. It starts with the module id. |
| `name`, `icon` | Shown in menus and cell headers |
| `description` | One or two plain sentences that say what an item of the type shows, such as "The web page at an address, signed in with the item's profile." The [Modules window](ui.md#modules-window) shows it under the type's name. Optional in the interface, but every type of the modules in this repository has one, and their tests check it. See [decision 0056](decisions/0056-modules-window.md). |
| `shortName` | Optional: a short label, one word if possible, such as "MR" or "Jira". Cell headers show it with the icon in the type pill. Defaults to `name`. See [decision 0033](decisions/0033-cell-header-pills.md). |
| `fields` | The configuration fields: key, label, type, default, whether it is required, and for a choice its values and labels. The core builds the item dialog's form from them and checks the values. See [decision 0028](decisions/0028-item-fields-in-m3.md). |
| `fieldsForProfile` | Optional: the fields for an item that uses a given [profile](data-model.md#profile), such as a field whose suggestions come from that profile's module settings. The item dialog builds its form from it. Defaults to `fields`. See [decision 0050](decisions/0050-profiles-hold-logins-connections-and-settings.md). |
| `createEditor` | Optional: an editor of the type's own for the configuration, used instead of the form built from `fields`. It receives the project's folders and a way to save an image into `files/`. |
| `pastedImageField` | Optional: the key of an image field that a pasted image fills. If set, "Paste Image…" in a task's Items menu creates an item of this type, and `Ctrl+V` replaces the image of such an item. See [Image](#image). |
| `rendering` | `native` (a Qt widget) or `web` (a [web view](web-views.md)). The [memory policy](web-views.md#memory-policy) unloads web views. |
| `viewKeys` | Optional: configuration keys the view writes itself, such as a note's text. A change to only these keys keeps the view, which is told of it, instead of making it again. See [decision 0081](decisions/0081-views-that-keep-their-own-keys.md). |

| `status` | Optional: how to fetch status and attention from the [status cache](integrations.md#polling-and-caching) |
| `actions` | Optional: the light actions it offers |
| `fromUrl` | Optional: recognises a pasted web address and returns a configuration, a suggested name and the base URL of its server. See [items from a link](#items-from-a-link). |
| `createView` | Creates the item's view for one placement. See [the view](#the-view). |
| `singleView` | Optional, off by default: the item can have only one view at a time |
| `reparentSafe` | Optional, on by default: the view can be moved to another cell without being created again |

A new item starts with a grid of the registered types, where the user picks one, then edits the item of that type; see [the item dialog](ui.md#item-dialog). The item dialog keeps the configuration keys that its form or editor does not show. For a type that uses a [profile](data-model.md#profile), the dialog also offers a **Profile** choice: "<name> (from the group)", such as "Work (from the group)", which is the default, or any profile of the list. A type uses a profile when its `rendering` is web or its module has settings screens. The choice is stored in the item's `profile`, not in its configuration. See [web views](web-views.md#profiles). An item whose module is missing has neither fields nor an editor; its dialog edits the name only and keeps the configuration unchanged.

New types are added by registering a class in a module. No core code changes. See [adding a module](modules.md#adding-a-module).

When a field has a project default, the type uses it when the field is empty. For example, an empty project path in an MR List uses the project's `gitlab.project` setting. See [module settings](data-model.md#project).

## Items from a link

A pasted web address can make an item. The new item dialog asks every type's `fromUrl` about it, and offers the types that take it: types of modules first, then Web page. Choosing one fills its configuration and name. Each type reads the address with the server in its module settings, in the profile of the group. See [decision 0049](decisions/0049-items-from-a-pasted-link.md), [the item dialog](ui.md#item-dialog) and [modules](modules.md#items-from-a-link).

| Address | Types that take it | Configuration filled | Name |
|---|---|---|---|
| `<base>/<project>/-/merge_requests/<iid>` | GitLab MR (summary), GitLab MR (page), GitLab CI Pipeline | `project`, `iid`; the pipeline also `branchSource` = `mergeRequest` | `group/project!42`; the pipeline `group/project!42 CI` |
| `<base>/<project>/-/issues/<iid>` | GitLab Issue (summary), GitLab Issue (page) | `project`, `iid` | `group/project#7` |
| `<base>/<project>/-/merge_requests`, `<base>/<project>/-/issues` | GitLab MR List, GitLab Issue List | `scope` = `project`, `project`, `preset` = `allOpen` | `group/project MRs`, `group/project issues` |
| `<base>/<project>/-/pipelines[/<id>][?ref=<branch>]` | GitLab CI Pipeline | `project`, `branchSource` = `fixed`, `branch` from `ref`, or `master` without one. The pipeline's id is not kept, because the type follows a branch. | `group/project <branch> CI` |
| `<base>/<project>/-/pipelines`, without `ref` or an id | GitLab CI Pipeline List, after GitLab CI Pipeline | `project`, `source` = `all` | `group/project pipelines` |
| `<base>/browse/<key>`, `.../issues/<key>`, or `?selectedIssue=<key>` | Jira Issue (summary), Jira Issue (page) | `key` | the key |
| a Jira address with `?jql=<query>` | Jira List | `preset` = `jql`, `jql` | "Jira search" |
| `https://claude.ai/...` | Claude session (web) | `url` | "Claude session" |
| any `http` or `https` address | Web page | `url` | the host, without `www.` |

- **GitLab servers.** `<base>` is the GitLab base URL of the group's profile, so a server installed at a path is read correctly. On another server, `gitlab.com` and any other server are still read, because the `/-/` in the path is GitLab's own; the base is then the server without a path.
- **Jira servers.** `<base>` is the Jira base URL of the group's profile, or any `atlassian.net` site. Any other server is read only from `/browse/<key>`, with Jira taken to be installed at the path before it. Keys are read in upper case.

## The view

`createView` returns a plain `QWidget`. The item type may put anything inside it, such as Qt widgets, a web view, or a container around a native window made with `QWidget::createWindowContainer`. The core places the widget in a cell, a tab or a detached window, and never looks inside it. See [decision 0016](decisions/0016-generic-items.md).

Two flags tell the core what the view cannot do. Both default to what an ordinary widget can do.

- **`singleView`.** Normally each placement gets its own view, as in [decision 0003](decisions/0003-view-per-placement.md). Some content can exist only once, such as a device camera or a window from a library that allows one instance. When `singleView` is on, only one placement shows the view. The others show "Shown in another cell", with a button to show it there instead.
- **`reparentSafe`.** When a layout is edited, `LayoutView` keeps each view and moves it to its new parent widget, so a web page keeps its state. Some views, such as containers around native windows, do not survive that move. When `reparentSafe` is off, `LayoutView` creates the view again after an edit instead of moving it. The view loses its state.

## Light actions

Native views are read-mostly, as in [decision 0070](decisions/0070-read-mostly-native-views.md). The only actions are for CI, because running CI is repeatable and changes no one's review or issue:

- retry a pipeline or a single job;
- cancel a pipeline, after a confirmation;
- play a manual job;
- trigger a new pipeline on the branch, after a confirmation, since it starts work.

Anything else, such as approving or commenting, uses the "Open page" button. It opens the matching page type in the same cell, or in the external browser if the user holds `Shift`.

## GitLab

Module `gitlab`. All GitLab types talk to the GitLab server of the item's profile, set in its GitLab General Configuration: a base URL, `https://gitlab.com` by default, and a token. In the item dialog, every `project` field offers the project paths of that screen in an editable list, so a path is chosen instead of typed. Another path can still be typed. See [settings screens](modules.md#settings-screens).

The summaries have fixed layouts, as in [decision 0066](decisions/0066-fixed-native-layouts-with-sections.md). The title, the state and the actions always show, and scroll with the rest. Each other section has a field in the item's configuration, which is on when absent. These rules hold for all of them:

- With no data yet, a summary shows "Loading…" or the status's message. With old data and an error, it keeps the data, and the cell header shows the problem.
- After an action succeeds, the status is fetched again. A failed action shows its message in the action row. A `403` says that the token cannot do this, and names the `api` scope.
- Only cancelling and triggering a pipeline ask first.
- "Open page" shows the page in the same cell, as in [decision 0069](decisions/0069-open-page-in-the-placement.md), or in the external browser if the user holds `Shift`.
- **The Web link** is a "Web" button at the end of the actions. It opens the same address in the external browser, and it hides without a web address. The address is `web_url` in the data. Without one, for a merge request or an issue, it is built from the base URL and the configuration, as for the page types. A link to a job's log also opens in the external browser.
- Dates are local and short. Text from the server is plain text, so it is never read as markup.

### GitLab MR (summary)

`gitlab.mr.summary`, native.

- **Configuration:** `project`, the project path, which defaults to the project's `gitlab.project` setting when empty; `iid`, the merge request number, required. Pasting a merge request URL fills both.
- **Top:** the title; pills for the state and for a draft; the source and target branches.
- **Sections and fields,** in this order:
  - Pipeline (`showPipeline`): the head pipeline's status and id, with a "Web" link to its page, and its failed jobs, each with its duration and a link to its log. A job that is allowed to fail says so.
  - Approvals (`showApprovals`): the count, and who has approved.
  - People (`showPeople`): the author, assignees and reviewers.
  - Labels (`showLabels`).
- **Actions:**
  - "Retry pipeline", when the head pipeline failed or was canceled;
  - "Open page" and "Web".
- **Status:** a summary such as "Open · 2 of 3 approvals · pipeline failed". The item is closed when the merge request is merged or closed.
- **Attention,** only while the merge request is open. "You" is the owner of the profile's token.
  - "Pipeline failed" when the head pipeline failed, with a desktop notification;
  - "Review requested" when you are a reviewer and have not approved;
  - "Approved" when you are the author and the merge request is approved. It clears once you have seen the item;
  - "Has conflicts" when you are the author and the merge request has conflicts.

### GitLab MR (page)

`gitlab.mr.page`, web. The merge request's web page, in the item's [profile](web-views.md#profiles). Its configuration is the summary's: `project` and `iid`.

- **Address:** `<baseUrl>/<project>/-/merge_requests/<iid>`, where `baseUrl` is the GitLab base URL of the item's profile. An empty `project` uses the project's `gitlab.project` setting.
- **With a base URL that is not a web address,** the cell says "Set the GitLab base URL in this profile's Modules > GitLab > General Configuration, in the Profiles menu."
- **Without a project or a number,** the cell says "Set the project and number in the item's settings."
- **Status and attention** are the summary's, so a page in a cell shows the same badge. See [decision 0061](decisions/0061-attention-from-status-of-any-rendering.md).

### GitLab MR List

`gitlab.mr.list`, native.

- **Configuration:**
  - `scope`: `project` (one project), `group` (one GitLab group) or `all` (all projects visible to you);
  - `project`, the project path, empty for the `gitlab.project` setting; `group`, the group path;
  - `preset`: `createdByMe` ("Created by me"), `reviewRequested` ("Review requested from me"), `assignedToMe` ("Assigned to me") or `allOpen` ("All open");
  - `filter`: optional custom filter parameters, passed to the GitLab merge requests API, such as `labels=bug&target_branch=main`;
  - `limit`: how many rows it fetches, 100 by default;
  - `pageSize`: how many rows show at a time, 50 by default, with Previous and Next under the table. Sorting covers all fetched rows;
  - `showPipeline` and `showApprovals`: they show those two columns, and are on by default.

  An item without `limit` or `pageSize`, or with a value below 1, uses the default.
- **Shows:** a sortable table, sorted on Updated, newest first. Its columns are:
  - Ref, such as `backend!42`, with the last part of the project path;
  - Title, which takes the free width and never hides;
  - Author;
  - Pipeline, a pill with the head pipeline's status;
  - Approvals, such as `1/2`, or just the count when none are required;
  - Updated, compact, such as `10-30 14:32`, or `2025-10-30` for another year, sorted by the date;
  - the links of the row, which never hide.
- **A narrow cell** hides columns in this order: Author, Approvals, Updated, Pipeline, Ref.
- **Pipeline and Approvals** are filled by the view, for the first 25 rows of the current page. It asks while it is visible, after each new status, each time it shows, and when a new page, sort or filter brings other rows. What is on the way for a row that leaves is dropped. The other rows show a dash, and so does a row whose request failed. A cell keeps its last answer until the new one comes. A hidden column asks for nothing. See decisions [0067](decisions/0067-lists-report-status.md) and [0071](decisions/0071-lists-link-out-without-a-preview.md).
- **Links of a row:**
  - "Web" opens the merge request's page in the external browser;
  - "Open" shows the page in the same cell, or in the external browser if you hold `Shift`;
  - "Add" adds a GitLab MR (summary) to the item's task, with a name such as `group/project!42`.
- **Row menu:** "Web", "Open page", "Add to task as item" and "Copy link".
- **The Web link** is a "Web" button above the table. It opens the list's page on the site, in the external browser, and it hides when the scope is not complete: `<baseUrl>/<project>/-/merge_requests`, `<baseUrl>/groups/<group>/-/merge_requests` or `<baseUrl>/dashboard/merge_requests`.
- **Messages:** with no data it says "Loading…" or the status's message. With old data and an error it keeps the rows, because the cell header shows the problem. An empty list shows the table with no rows and the line "No merge requests".
- **Status:** a summary with the count, such as "12 merge requests", or "100+ merge requests" when the list reaches its `limit`. It reports no attention, and is neither closed nor open, so it never counts towards a task being done. A scope without its project or group reports no status, with a message such as "Set the group in the item's settings." See [decision 0067](decisions/0067-lists-report-status.md).

### GitLab CI Pipeline

`gitlab.pipeline`, native.

- **Configuration:**
  - `project`, the project path, empty for the `gitlab.project` setting;
  - `branchSource`, with three choices: `fixed` (the branch in `branch`, `master` by default), `mainFolder` ("current branch of the main folder"), or `mergeRequest` ("branch of a linked merge request", the merge request numbered `iid`).
- **The group.** The pipelines created within 10 minutes of the latest, as in [decision 0072](decisions/0072-pipelines-started-together.md). Some repositories start several at once for one merge request.
- **Top:** the pipeline shown, at first the latest: its id; a pill for its status; the branch, the duration and when it was created. An entry of GitLab's lists has no duration, so only the latest shows one.
  - With more than one in the group, a row of pills, one per pipeline in the group, switches between them. The shown one is bold.
  - "Previous" and "Next" step through all of the latest 20 pipelines, also those before the group. "Previous" goes back in time. The view reads an older pipeline's jobs when it shows.
  - A pipeline the user chose stays shown through a new status while it is among those 20. Otherwise the latest shows, and a newer latest replaces it.
- **Jobs:** one table, with a heading for each stage, in the order the jobs were created. Each row has a pill for the job's status, the name, the duration and links:
  - "Log", on any job that has run. It opens in the external browser;
  - "Retry", on a finished job: passed, failed or canceled;
  - "Play", on a manual job.
- **Actions:**
  - "Retry pipeline", when the pipeline failed or was canceled;
  - "Cancel pipeline", while it can still be cancelled, after a confirmation;
  - "Trigger pipeline", which starts a new pipeline on the branch, after a confirmation;
  - "Merge request", which opens it in the external browser: the merge request `iid` of the configuration when `branchSource` is `mergeRequest`, otherwise the open merge request of that source branch. It hides when there is none;
  - "Open page" and "Web", on the page of the pipeline shown.
- **Refresh:** while the pipeline shown is unfinished and the view is visible, the view asks for a new status every 15 seconds. A pipeline that waits on a manual job counts as finished here, since it waits for a person. Otherwise the usual interval holds.
- **Messages:** a branch without pipelines shows "No pipeline on <branch>" as its title, with only "Trigger pipeline" and the merge request's link. A pipeline that follows the main folder shows its status's message, in place of everything else.
- **Status:** a summary of the latest, such as "Failed · main · 1 failed job", or "No pipeline on main" for a branch without pipelines. The item is never closed. A pipeline that follows the main folder reads the folder's branch at each fetch, and reports no status, with a message, when the folder has no path on this device or is not on a branch.
- **Attention:** a pipeline of the group failed, whichever is shown. Each failed pipeline is a new reason with its own key, so a failure is not hidden by a later pipeline that passed, and a retry that fails again counts again. Together with the merge request's failed pipeline, this is the only event that sends a [desktop notification](ui.md#tray-and-notifications).

"Current branch of the main folder" reads `.git/HEAD` of the folder's local path on this device. If the folder is not mapped on this device, the panel says "Set the main folder's path on this device."

### GitLab CI Pipeline List

`gitlab.pipeline.list`, native. The pipelines of one project.

- **Configuration:**
  - `project`, the project path, empty for the `gitlab.project` setting;
  - `source`: `all` ("All pipelines", the default), `mine` ("Started by me"), `branch` ("Of a branch") or `mergeRequest` ("Of a merge request");
  - `branch`, used by `branch`. It is `master` by default, also as its placeholder, and an empty value means `master`;
  - `iid`, the merge request number, used by `mergeRequest`;
  - `limit` and `pageSize`, as for the merge request list.
- **Shows:** a sortable table, sorted on Id, newest first. Its columns are:
  - Id;
  - Status, a pill;
  - Ref, which takes the free width and never hides;
  - Author, which GitLab's list answers usually leave out, so it usually shows a dash;
  - Started, compact and sorted by the date, or the time the pipeline was created when GitLab gives no start. A missing date sorts as the oldest;
  - the links of the row, which never hide.
- **A narrow cell** hides columns in this order: Author, Started, Id, Status.
- **Filter:** a Status drop-down above the table, filled from the fetched rows. The choice survives new rows and is not saved.
- **Links of a row:**
  - "Web" opens the row's `web_url` in the external browser;
  - "Open" shows that page in the same cell, or in the external browser if you hold `Shift`;
  - "Add" adds a GitLab CI Pipeline to the item's task. A row whose ref is `refs/merge-requests/<iid>/head`, `/merge` or `/train` adds one with `branchSource` `mergeRequest` and that `iid`, named `group/project!42 CI`. Any other ref adds one with `branchSource` `fixed` and that ref as its `branch`, named `group/project <ref> CI`.
- **Row menu:** "Web", "Open page", "Add to task as item" and "Copy link".
- **The Web link** above the table opens `<baseUrl>/<project>/-/pipelines`, and it hides without a project.
- **Messages:** as for the merge request list, with the line "No pipelines".
- **Status:** a summary with the count, such as "12 pipelines", "1 pipeline", or "100+ pipelines" when the list reaches its `limit`. It reports no attention and is neither closed nor open. Without a project, or without a number for `mergeRequest`, it reports no status, with a message such as "Set the merge request number in the item's settings."

### GitLab Issue (summary), GitLab Issue (page), GitLab Issue List

`gitlab.issue.summary`, `gitlab.issue.page` and `gitlab.issue.list` follow the merge request pattern:

- **The summary's top:** the title and a pill for the state.
- **The summary's sections and fields,** in this order:
  - Details (`showDetails`): the assignees, milestone and due date.
  - Labels (`showLabels`).
  - Comments (`showComments`): the latest five, newest first, with their author and date, as plain text. It says "No comments" when there are none, and "The comments could not be read." when the request failed.
- **Configuration:** the summary and the page have `project` and `iid`, the issue number. The list has `scope`, `project`, `group`, `preset`, `filter`, `limit` and `pageSize`, as for merge requests.
- **The list presets are:** `assignedToMe` ("Assigned to me"), `createdByMe` ("Created by me") and `allOpen` ("All open"). Custom filters work the same way as for merge requests.
- **The list shows** a sortable table with the columns Ref, such as `backend#7`, Title, Assignees, Labels, Updated and the links, sorted on Updated, newest first. A narrow cell hides columns in this order: Labels, Assignees, Updated, Ref. It makes no further requests. The pages, the links, the row menu, the Web link and the messages follow the merge request list, with a GitLab Issue (summary) added to the task with a name such as `group/project#7`, the address ending in `/issues`, and the line "No issues".
- **The page's address** is `<baseUrl>/<project>/-/issues/<iid>`. Its messages are those of the merge request page.
- **The summary's actions:** "Open page" and "Web".
- **Status:** the summary and the page report a summary such as "Open · assigned to Ana", and are closed when the issue is closed. They report no attention. The list reports as the merge request list does, with a summary such as "12 issues" or "100+ issues". See [decision 0067](decisions/0067-lists-report-status.md).

## Jira

Module `jira`. Jira types talk to the Jira site of the item's profile, set in its Jira General Configuration: a base URL, without a default, the deployment, the email for Jira Cloud, a token, and any extra fields. See [integrations](integrations.md#jira).

**Extra fields** are field names, one per line, such as "Team" and "Fix version". Summaries and lists show them, found by name, ignoring case and a final "s". A name the site does not have is left out quietly. Values are plain text: an object reads as its name, and a list is joined with commas. See [decision 0073](decisions/0073-jira-extra-fields.md).

A project can set its Jira project key in the project dialog's Jira section, stored as `jira.projectKey`. The list presets of the project use it.

### Jira Issue (summary)

`jira.issue.summary`, native.

- **Configuration:** `key`, the issue key, such as `PROJ-123`, required. Pasting an issue URL fills it. `showDetails`, `showDescription`, `showComments` and `showLinks` each show a section, and are on by default and when absent.
- **Shows:** at the top, always, the key and summary, pills for the status and the type, and the actions. They scroll with the rest. Then these sections, in this order:
  - Details: priority, assignee, reporter, sprint and the extra fields;
  - Description: rendered from the HTML in `renderedFields`;
  - Comments: the latest five, newest first;
  - Sub-tasks and links: each with its key, which opens the issue in the external browser, its relation, summary and status. Then the issues whose parent is this one, such as the issues of an epic, with their status pills: up to 50, with a line saying how many more there are. A sub-task is not listed twice.
- **Rendered HTML.** It loads nothing, neither images nor style sheets, so a description cannot call out. A link opens only in the external browser, resolved against the base URL, and only for `http` and `https`. All other text from the server is plain text.
- **Actions:** Open page, or the external browser with `Shift`; and the Web link, `<baseUrl>/browse/<key>`. The view only reads. See [decision 0070](decisions/0070-read-mostly-native-views.md).
- **Without data,** the view shows "Loading…" or the status's message. With old data and an error, it keeps the data, and the cell header shows the error.
- **Status:** from M5. One request for the issue with all its fields, and the names of the fields, which tell which one is the sprint. A further search asks for its children, and if only that fails, the status stands without them. The status line reads like "In Review · assigned to Ana". The item counts as closed when the status category is `done`.
- **Attention:** "Status changed to <name>", with the key `status:<status id>`, when the status changed since the user last viewed the item. It clears once seen, and the status an issue had when it was added does not count. See [decision 0061](decisions/0061-attention-from-status-of-any-rendering.md).

### Jira Issue (page)

`jira.issue.page`, web. The issue's web page. Its configuration is the summary's: `key`.

- **Address:** `<baseUrl>/browse/<key>`, where `baseUrl` is the Jira base URL of the item's profile.
- **Without a Jira base URL,** the cell says "Set the Jira base URL in this profile's Modules > Jira > General Configuration, in the Profiles menu."
- **Without an issue key,** the cell says "Set the issue key in the item's settings."
- **Status and attention:** the same as the summary's, from a smaller request: the issue's summary, status, assignee, priority, type and last update.

### Jira List

`jira.list`, native.

- **Configuration:** `preset`, one of these presets, or `jql` for the JQL query (Jira's query language) in the `jql` field:
  - `myOpen`, "My open issues";
  - `currentSprint`, "Current sprint of this project", using the project's `jira.projectKey` setting;
  - `recentlyUpdated`, "Recently updated in this project".

  `limit` sets how many rows it fetches, 100 by default, and `pageSize` how many the view shows at a time, 50 by default. Both use the default when absent or below 1, so older items need no change.
- **Queries:**
  - `myOpen`: `assignee = currentUser() AND resolution = Unresolved ORDER BY updated DESC`;
  - `currentSprint`: `project = "<key>" AND sprint in openSprints() ORDER BY rank`;
  - `recentlyUpdated`: `project = "<key>" AND updated >= -7d ORDER BY updated DESC`.

  The key is quoted, so a value cannot change the query.
- **Shows:** a sortable table with the columns Key, Summary, Status, Assignee, Updated, the extra fields and links, sorted on Updated, newest first. The extra fields hide first in a narrow cell. The status is a pill: green for the category `done`, amber for `indeterminate`, grey otherwise. Updated reads like `10-30 14:32`, or `2025-10-30` for another year, and sorts by the real date. The Web link above the table opens `<baseUrl>/issues/?jql=<query>` in the external browser. Previous and Next page through the fetched rows, and a drop-down filters them by status. See [decision 0071](decisions/0071-lists-link-out-without-a-preview.md).
- **Row links:** "Web", the issue's page in the external browser; "Open", the page in the cell, or the external browser with `Shift`; and "Add", which adds a `jira.issue.summary` item for the issue to the task. The column never hides.
- **Row menu:** the same, and "Copy link".
- **Status:** the rows, up to `limit`, and their number, in one search. The status line reads like "12 issues", "1 issue", or "100+ issues" at the limit, which is the item's `limit`. A list raises no attention, and never counts as closed. See [decision 0067](decisions/0067-lists-report-status.md).
- **Without a project key,** the two project presets send nothing, and the cell says "Set the Jira project key in the project's settings." An empty `jql` says "Set the JQL query in the item's settings."

## Claude

Module `claude`.

### Claude session (web)

`claude.session.web`, web.

- **Configuration:** `url`, a claude.ai URL, such as a Remote Control or cloud session link, required.
- **Shows:** the claude.ai page, in the item's [profile](web-views.md#profiles). If `url` is empty or is not an `http` or `https` address, the cell says "Set the address in the item's settings."

There is no public API for these sessions, so the app has no status or summary for them. See [decision 0008](decisions/0008-claude-sessions.md).

### Claude session (local), removed

M7 added `claude.session.local`, a Claude Code session resumed in a terminal. [Decision 0084](decisions/0084-no-local-claude-sessions.md) removed it. An item of that type that is still in a workspace is kept unchanged. Its cell says "This item type is no longer available: claude.session.local.", and the user can delete it.

## General

### Web page

`web.page`, web, module `web`.

- **Configuration:** `url`, the page's address, required.
- **Shows:** the page, in the item's [profile](web-views.md#profiles). If `url` is empty or is not an `http` or `https` address, the cell says "Set the address in the item's settings."

### Notes

`notes`, native, module `notes`.

- **Configuration:** none that the item dialog shows; it edits only the name. The text is kept in the configuration under `text`, which the dialog keeps unchanged.
- **Shows:** a Markdown editor and its preview, with an **Edit** and a **Preview** button above them. It opens in Preview when the note has text, else in Edit. `Ctrl+E` switches while the keyboard's focus is in the note. The preview loads no images or other resources, and opens web and mail links in the external programs.
- **Saving:** the text is stored in the task file, so it syncs. It is saved 800 ms after the last change, when the editor loses the keyboard's focus, and when the view closes. Empty text removes `text`. `viewKeys` is `text`, so saving does not make the editor again. A change from another device replaces the text only when the note has no unsaved change; otherwise the local text wins when it is saved. See [decision 0081](decisions/0081-views-that-keep-their-own-keys.md).

### Image

`image`, native, module `image`. See [decision 0021](decisions/0021-image-items-as-files.md).

- **Configuration:** `file`, the image's file in the data folder's `files/` folder, required; `link`, an optional URL; `memo`, optional plain text. The type returns `file` from `pastedImageField`.
- **Shows:** the image, scaled to fit the cell with its aspect ratio kept, and centred, with the memo under it as plain, selectable text. The memo is hidden when empty. With an `http` or `https` link, the pointer is a hand over the image, the tooltip is the link, and a click opens it in the external browser. A missing file shows "The image file is missing.", and a file that is not an image "The image file cannot be read."
- **Paste:** an image on the clipboard is pasted in one of two ways. The [paste dialog](ui.md#pasting-images) opens first, to preview the image and crop it. The cropped image is saved as PNG in `files/`, and the item's `file` is set to the new file's name. `ImagePaste` does the work, and the status bar shows its message. The "Paste" button of the item dialog's image field opens the same dialog. A pasted image is never added straight to a dashboard. See [decision 0051](decisions/0051-paste-images-from-the-items-menu.md).
  - **Paste Image…**, in the context menu of a task or its Items row, adds a new Image item to that task, named "Image", or "Image 2", "Image 3" and so on if the task already has an item with that name. No dashboard changes. The user drags the item from the sidebar onto a dashboard if wanted.
  - `Ctrl+V` replaces the image of the Image item in the focused cell, or of the item shown alone in a tab. This is an item edit, so it works on a locked dashboard, and in a root dashboard cell that shows the item. The old file is deleted when no item refers to it any more ([decision 0030](decisions/0030-deleting-image-files.md)). On any other cell, or without a focused cell, nothing changes, no dialog opens, and the status bar says "To add an image, use Paste Image… in a task's Items menu."
  - The new item is of the first type, by name, whose `pastedImageField` is set. In a build without such a type, such as one without the `image` module, nothing is pasted and the status bar says "This build cannot show images."
  - If the image cannot be saved, for example in the read-only sample data, nothing changes and the status bar says "The image could not be saved." For the read-only sample data this shows before any dialog opens.
- **Drop:** an image file or image dropped from another app goes where the same image would go by pasting: on a task or its Items row in the sidebar, a new item as Paste Image… adds; on an Image item's cell, the item's image is replaced as `Ctrl+V` does; on the item dialog's image field, as its "Paste" button. The paste dialog opens first, and the rules above apply. `ImagePaste::imageFromMime()` reads the drop. See [ui](ui.md#pasting-images) and [decision 0085](decisions/0085-drop-images-from-other-apps.md).

### Folder / terminal

`folder`, native, module `folder`.

- **Configuration:** `folder`, one of the project's folders, required.
- **Shows:**
  - the folder's name, which is the last part of its local path, and the path;
  - the current Git branch, or "Detached at <commit>";
  - one line of `git status`, such as "2 ahead · 3 changed · 1 untracked", without the counts that are zero, or "Clean". Outside a repository it says "Not a Git repository", and without Git "Git was not found"; the branch still comes from the files.
- **Actions:** open a terminal, open in the editor, open in the file manager. They work whatever Git says. A failure shows its message under them.

The Git state is read when the view shows and every 10 seconds while it is visible. A folder with no local path on this device shows "Folder not set on this device" with **Choose folder…** instead of the actions.

The editor command is a device setting of the `folder` module, default `code`. It is split like a command line: its first part is the program, the rest its arguments, and the folder's path the last argument. The terminal command is a core device setting. See [decision 0082](decisions/0082-commands-as-argument-lists.md).

### Built-in panels

`dash.panel`, web, served from `dash://`. Part of the core, not a module. See [web views](web-views.md#built-in-panels).

- **Configuration:** `panel`, which panel, required. Two exist:
  - **Task summary** (`task-summary`): the cached status of every item in the task that reports one, on one page.
  - **Group overview** (`group-overview`): active tasks of the group of the panel's task, with their priorities and attention badges.
- **Module:** the core's own module `dash`, "Built in". It is part of every build. See [decision 0094](decisions/0094-built-in-panels-as-a-core-item-type.md).
- **Profile:** none to choose. Panels load in the app profile, so the item dialog offers neither a profile nor keys for the page.

- **Shows:**
  - **Task summary:** the task's title, its group and project, then one row per item whose type reports a status, in the task's order. A row has the type's short name, the item's name, the age of its status and its summary. Without a status yet it says "Not fetched yet", and in an error state, such as a missing token, the state's message in grey. An item that needs attention has a red left edge and its reasons under it. Without such items it says "No items in this task report a status."
  - **Group overview:** the group's name, then one row per active task of the group, not in the trash, by priority, then in the sidebar's order. A row has the priority's colour, the title, the project, a red attention count when it is not zero, and "This task" on the panel's own task. Without tasks it says "No active tasks in this group."
- **Actions:** a click on a row, or Enter on a row reached with Tab, opens the item alone or the task in the preview.

A panel draws again when something it shows changes, and once a minute for the ages. An unknown `panel` value shows "Choose the panel in the item's settings." instead of a page.
