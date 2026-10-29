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
| `shortName` | Optional: a short label, one word if possible, such as "MR" or "Jira". Cell headers show it with the icon in the type pill. Defaults to `name`. See [decision 0033](decisions/0033-cell-header-pills.md). |
| `fields` | The configuration fields: key, label, type, default, whether it is required, and for a choice its values and labels. The core builds the item dialog's form from them and checks the values. See [decision 0028](decisions/0028-item-fields-in-m3.md). |
| `createEditor` | Optional: an editor of the type's own for the configuration, used instead of the form built from `fields`. It receives the project's folders and a way to save an image into `files/`. |
| `pastedImageField` | Optional: the key of an image field that a pasted image fills. If set, pasting an image onto a task dashboard creates an item of this type. See [Image](#image). |
| `rendering` | `native` (a Qt widget) or `web` (a [web view](web-views.md)) |
| `connectionKind` | The kind of [connection](data-model.md#connection) it needs, if any |
| `status` | Optional: how to fetch status and attention from the [status cache](integrations.md#polling-and-caching) |
| `actions` | Optional: the light actions it offers |
| `fromUrl` | Optional: recognises a pasted URL and returns a configuration |
| `createView` | Creates the item's view for one placement. See [the view](#the-view). |
| `singleView` | Optional, off by default: the item can have only one view at a time |
| `reparentSafe` | Optional, on by default: the view can be moved to another cell without being created again |

The item dialog keeps the configuration keys that its form or editor does not show. An item whose module is missing has neither fields nor an editor; its dialog edits the name only and keeps the configuration unchanged.

New types are added by registering a class in a module. No core code changes. See [adding a module](modules.md#adding-a-module).

When a field has a project default, the type uses it when the field is empty. For example, an empty project path in an MR List uses the project's `gitlab.project` setting. See [module settings](data-model.md#project).

## The view

`createView` returns a plain `QWidget`. The item type may put anything inside it, such as Qt widgets, a web view, or a container around a native window made with `QWidget::createWindowContainer`. The core places the widget in a cell, a tab or a detached window, and never looks inside it. See [decision 0016](decisions/0016-generic-items.md).

Two flags tell the core what the view cannot do. Both default to what an ordinary widget can do.

- **`singleView`.** Normally each placement gets its own view, as in [decision 0003](decisions/0003-view-per-placement.md). Some content can exist only once, such as a device camera or a window from a library that allows one instance. When `singleView` is on, only one placement shows the view. The others show "Shown in another cell", with a button to show it there instead.
- **`reparentSafe`.** When a layout is edited, `LayoutView` keeps each view and moves it to its new parent widget, so a web page keeps its state. Some views, such as containers around native windows, do not survive that move. When `reparentSafe` is off, `LayoutView` creates the view again after an edit instead of moving it. The view loses its state.

## Light actions

Native summaries offer a few actions that save opening the web page:

- retry a pipeline or a single job;
- cancel a pipeline;
- approve a merge request;
- move a Jira issue to another status;
- add a comment to a merge request, GitLab issue or Jira issue.

Anything else uses the "Open page" button. It opens the matching page type in the same cell, or in the external browser if the user holds `Shift`.

## GitLab

Module `gitlab`. All GitLab types need a `gitlab` connection.

### GitLab MR (summary)

`gitlab.mr.summary`, native.

- **Configuration:** `project`, the project path, which defaults to the project's `gitlab.project` setting when empty; `iid`, the merge request number, required. Pasting a merge request URL fills both.
- **Shows:**
  - title, state, draft flag;
  - source and target branches;
  - author, assignees, reviewers, labels;
  - latest pipeline status, with the failed jobs listed;
  - approvals: the count required and who has approved.
- **Actions:** approve, retry pipeline, add comment.
- **Attention:** pipeline failed, review requested from you, approved, has conflicts.

### GitLab MR (page)

`gitlab.mr.page`, web. The merge request's web page, using the group's [profile](web-views.md#profiles). Its configuration is the summary's: `project` and `iid`.

- **Address:** `<baseUrl>/<project>/-/merge_requests/<iid>`, where `baseUrl` is the connection's base URL. An empty `project` uses the project's `gitlab.project` setting.
- **Without a `gitlab` connection,** the cell says "No GitLab connection in this group."
- **Without a project or a number,** the cell says "Set the project and number in the item's settings."

### GitLab MR List

`gitlab.mr.list`, native.

- **Configuration:**
  - `scope`: `project` (one project), `group` (one GitLab group) or `all` (all projects visible to you);
  - `project`, the project path, empty for the `gitlab.project` setting; `group`, the group path;
  - `preset`: `createdByMe` ("Created by me"), `reviewRequested` ("Review requested from me"), `assignedToMe` ("Assigned to me") or `allOpen` ("All open");
  - `filter`: optional custom filter parameters, passed to the GitLab merge requests API, such as `labels=bug&target_branch=main`.
- **Shows:** a compact, sortable table with columns for reference, title, author, pipeline status, approvals, and the date of the last update.
- **Row click:** shows the merge request summary in a preview area below the table. A row menu offers "Add to task as item".

### GitLab CI Pipeline

`gitlab.pipeline`, native.

- **Configuration:**
  - `project`, the project path, empty for the `gitlab.project` setting;
  - `branchSource`, with three choices: `fixed` (the branch in `branch`), `mainFolder` ("current branch of the main folder"), or `mergeRequest` ("branch of a linked merge request", the merge request numbered `iid`).
- **Shows:** the latest pipeline for that branch:
  - stages, each with its jobs;
  - job status and duration;
  - a link to the log of each failed job.
- **Actions:** retry the pipeline, retry a job, cancel.
- **Attention:** the pipeline failed. This is the only event that sends a [desktop notification](ui.md#tray-and-notifications).

"Current branch of the main folder" reads `.git/HEAD` of the folder's local path on this device. If the folder is not mapped on this device, the panel asks for a branch.

### GitLab Issue (summary), GitLab Issue (page), GitLab Issue List

`gitlab.issue.summary`, `gitlab.issue.page` and `gitlab.issue.list` follow the merge request pattern:

- **The summary shows:** title, state, assignees, labels, milestone, due date and the latest comments.
- **Configuration:** the summary and the page have `project` and `iid`, the issue number. The list has `scope`, `project`, `group`, `preset` and `filter`, as for merge requests.
- **The list presets are:** `assignedToMe` ("Assigned to me"), `createdByMe` ("Created by me") and `allOpen` ("All open"). Custom filters work the same way as for merge requests.
- **The page's address** is `<baseUrl>/<project>/-/issues/<iid>`. Its messages are those of the merge request page.
- **Actions:** add comment, close and reopen.

## Jira

Module `jira`. Jira types need a `jira.cloud` or `jira.dc` connection.

### Jira Issue (summary)

`jira.issue.summary`, native.

- **Configuration:** `key`, the issue key, such as `PROJ-123`, required. Pasting an issue URL fills it.
- **Shows:**
  - key, summary, status, type, priority, assignee, reporter, sprint;
  - the description, rendered from the HTML in `renderedFields`;
  - the latest comments;
  - sub-tasks and linked issues;
  - development information: linked branches and merge requests, where the Jira instance provides it.
- **Actions:** change status, using the transitions Jira allows for the issue; add comment.
- **Attention:** the status changed since the user last viewed the item.

### Jira Issue (page)

`jira.issue.page`, web. The issue's web page. Its configuration is the summary's: `key`.

- **Address:** `<baseUrl>/browse/<key>`, where `baseUrl` is the connection's base URL. A `jira.cloud` connection is used first, then a `jira.dc` one.
- **Without a Jira connection,** the cell says "No Jira connection in this group."
- **Without an issue key,** the cell says "Set the issue key in the item's settings."

### Jira List

`jira.list`, native.

- **Configuration:** `preset`, one of these presets, or `jql` for the JQL query (Jira's query language) in the `jql` field:
  - `myOpen`, "My open issues";
  - `currentSprint`, "Current sprint of this project", using the project's `jira.projectKey` setting;
  - `recentlyUpdated`, "Recently updated in this project".
- **Shows:** a sortable table with columns for key, summary, status, assignee and the date of the last update.
- **Row click:** shows the issue summary in a preview area. A row menu offers "Add to task as item".

## Claude

Module `claude`.

### Claude session (web)

`claude.session.web`, web.

- **Configuration:** `url`, a claude.ai URL, such as a Remote Control or cloud session link, required.
- **Shows:** the claude.ai page, using the group's profile.

There is no public API for these sessions, so the app has no status or summary for them. See [decision 0008](decisions/0008-claude-sessions.md).

### Claude session (local)

`claude.session.local`, native.

- **Configuration:**
  - `sessionId`, the session id, required;
  - `folder`, one of the project's folders, the main folder when empty;
  - `device`, a note about which device the session lives on.
- **Shows:** the session id, folder and device, plus a "Resume in terminal" button.
- **Action:** runs `claude --resume <id>` in a terminal in the folder's local path. See [terminal launch](integrations.md#terminal-launch).

Local sessions exist on one device only. On other devices the panel shows the device name and disables the button.

## General

### Web page

`web.page`, web, module `web`.

- **Configuration:** `url`, the page's address, required.
- **Shows:** the page, using the group's profile, or the shared profile if the host is overridden.

### Notes

`notes`, native, module `notes`.

- **Configuration:** none. The item dialog edits only the name.
- **Shows:** a Markdown editor with a preview. The text is stored in the task file, so it syncs.

### Image

`image`, native, module `image`. See [decision 0021](decisions/0021-image-items-as-files.md).

- **Configuration:** `file`, the image's file in the data folder's `files/` folder, required; `link`, an optional URL; `memo`, optional plain text. The type returns `file` from `pastedImageField`.
- **Shows:** the image, scaled to fit the cell, with the memo under it. A click on the image opens the link, if there is one.
- **Paste:** `Ctrl+V` with an image on the clipboard. The image is saved as PNG in `files/`, and the item's `file` is set to the new file's name. `ImagePaste` does the work, and the main window shows its message in the status bar.
  - If the focused cell shows an Image item, or the item is shown alone in a tab, its image is replaced. This is an item edit, so it works on a locked dashboard, and in a root dashboard cell that shows the item. The old file is deleted when no item refers to it any more ([decision 0030](decisions/0030-deleting-image-files.md)).
  - Otherwise, on an unlocked dashboard, a new Image item is created, named "Image", or "Image 2", "Image 3" and so on if the task already has an item with that name. An empty focused cell holds it. Any other focused cell is split, with the new item on the right. Without a focused cell, a dashboard of one empty cell holds it; any other layout is split as a whole, with the new item on the right.
  - On a task dashboard the new item belongs to that task. A root dashboard has no task of its own, so a dialog asks for one among the active tasks, in sidebar order, with the task of the sidebar's selection chosen. The item is added to that task, and the root dashboard's cell refers to it with that task. Cancel pastes nothing. Without any active task, the status bar says "A pasted image belongs to a task. Add an active task first." See [decision 0032](decisions/0032-paste-images-into-root-dashboards.md).
  - On a locked dashboard, the status bar says "Unlock the dashboard to paste an image into it."
  - The new item is of the first type, by name, whose `pastedImageField` is set. In a build without such a type, such as one without the `image` module, nothing is pasted and the status bar says "This build cannot show images."
  - If the image cannot be saved, for example in the read-only sample data, nothing changes and the status bar says "The image could not be saved."

### Folder / terminal

`folder`, native, module `folder`.

- **Configuration:** `folder`, one of the project's folders, required.
- **Shows:**
  - the local path on this device;
  - the current Git branch;
  - a short `git status` summary.
- **Actions:** open a terminal, open in the editor, open in the file manager.

The editor command is a device setting of the `folder` module. The terminal command is a core device setting.

### Built-in panels

`dash.panel`, web, served from `dash://`. Part of the core, not a module. See [web views](web-views.md#built-in-panels).

- **Configuration:** which panel. Two exist:
  - **Task summary:** the cached status of every item in the task that reports one, on one page.
  - **Group overview:** active tasks of the group, with their priorities and attention badges.
