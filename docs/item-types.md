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

| `status` | Optional: how to fetch status and attention from the [status cache](integrations.md#polling-and-caching) |
| `actions` | Optional: the light actions it offers |
| `fromUrl` | Optional: recognises a pasted web address and returns a configuration, a suggested name and the base URL of its server. See [items from a link](#items-from-a-link). |
| `createView` | Creates the item's view for one placement. See [the view](#the-view). |
| `singleView` | Optional, off by default: the item can have only one view at a time |
| `reparentSafe` | Optional, on by default: the view can be moved to another cell without being created again |

A new item starts with a grid of the registered types, where the user picks one, then edits the item of that type; see [the item dialog](ui.md#item-dialog). The item dialog keeps the configuration keys that its form or editor does not show. For a type that uses a [profile](data-model.md#profile), the dialog also offers a **Profile** choice: "The group's profile (<name>)", which is the default, or any profile of the list. A type uses a profile when its `rendering` is web or its module has settings screens. The choice is stored in the item's `profile`, not in its configuration. See [web views](web-views.md#profiles). An item whose module is missing has neither fields nor an editor; its dialog edits the name only and keeps the configuration unchanged.

New types are added by registering a class in a module. No core code changes. See [adding a module](modules.md#adding-a-module).

When a field has a project default, the type uses it when the field is empty. For example, an empty project path in an MR List uses the project's `gitlab.project` setting. See [module settings](data-model.md#project).

## Items from a link

A pasted web address can make an item. The new item dialog asks every type's `fromUrl` about it, and offers the types that take it: types of modules first, then Web page. Choosing one fills its configuration and name. Each type reads the address with the server in its module settings, in the profile of the group. See [decision 0049](decisions/0049-items-from-a-pasted-link.md), [the item dialog](ui.md#item-dialog) and [modules](modules.md#items-from-a-link).

| Address | Types that take it | Configuration filled | Name |
|---|---|---|---|
| `<base>/<project>/-/merge_requests/<iid>` | GitLab MR (summary), GitLab MR (page), GitLab CI Pipeline | `project`, `iid`; the pipeline also `branchSource` = `mergeRequest` | `group/project!42`; the pipeline `group/project!42 CI` |
| `<base>/<project>/-/issues/<iid>` | GitLab Issue (summary), GitLab Issue (page) | `project`, `iid` | `group/project#7` |
| `<base>/<project>/-/merge_requests`, `<base>/<project>/-/issues` | GitLab MR List, GitLab Issue List | `scope` = `project`, `project`, `preset` = `allOpen` | `group/project MRs`, `group/project issues` |
| `<base>/<project>/-/pipelines[/<id>][?ref=<branch>]` | GitLab CI Pipeline | `project`, `branchSource` = `fixed`, `branch` from `ref`. The pipeline's id is not kept, because the type follows a branch. | `group/project <branch> CI` |
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

Native summaries offer a few actions that save opening the web page:

- retry a pipeline or a single job;
- cancel a pipeline;
- approve a merge request;
- move a Jira issue to another status;
- add a comment to a merge request, GitLab issue or Jira issue.

Anything else uses the "Open page" button. It opens the matching page type in the same cell, or in the external browser if the user holds `Shift`.

## GitLab

Module `gitlab`. All GitLab types talk to the GitLab server of the item's profile, set in its GitLab General Configuration: a base URL, `https://gitlab.com` by default, and a token. In the item dialog, every `project` field offers the project paths of that screen in an editable list, so a path is chosen instead of typed. Another path can still be typed. See [settings screens](modules.md#settings-screens).

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

`gitlab.mr.page`, web. The merge request's web page, in the item's [profile](web-views.md#profiles). Its configuration is the summary's: `project` and `iid`.

- **Address:** `<baseUrl>/<project>/-/merge_requests/<iid>`, where `baseUrl` is the GitLab base URL of the item's profile. An empty `project` uses the project's `gitlab.project` setting.
- **With a base URL that is not a web address,** the cell says "Set the GitLab base URL in this profile's Modules > GitLab > General Configuration, in the Profiles menu."
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

Module `jira`. Jira types talk to the Jira site of the item's profile, set in its Jira General Configuration: a base URL, without a default, and a token.

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

- **Address:** `<baseUrl>/browse/<key>`, where `baseUrl` is the Jira base URL of the item's profile.
- **Without a Jira base URL,** the cell says "Set the Jira base URL in this profile's Modules > Jira > General Configuration, in the Profiles menu."
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
- **Shows:** the claude.ai page, in the item's [profile](web-views.md#profiles). If `url` is empty or is not an `http` or `https` address, the cell says "Set the address in the item's settings."

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
- **Shows:** the page, in the item's [profile](web-views.md#profiles). If `url` is empty or is not an `http` or `https` address, the cell says "Set the address in the item's settings."

### Notes

`notes`, native, module `notes`.

- **Configuration:** none. The item dialog edits only the name.
- **Shows:** a Markdown editor with a preview. The text is stored in the task file, so it syncs.

### Image

`image`, native, module `image`. See [decision 0021](decisions/0021-image-items-as-files.md).

- **Configuration:** `file`, the image's file in the data folder's `files/` folder, required; `link`, an optional URL; `memo`, optional plain text. The type returns `file` from `pastedImageField`.
- **Shows:** the image, scaled to fit the cell, with the memo under it. A click on the image opens the link, if there is one.
- **Paste:** an image on the clipboard is pasted in one of two ways. The [paste dialog](ui.md#pasting-images) opens first, to preview the image and crop it. The cropped image is saved as PNG in `files/`, and the item's `file` is set to the new file's name. `ImagePaste` does the work, and the status bar shows its message. The "Paste" button of the item dialog's image field opens the same dialog. A pasted image is never added straight to a dashboard. See [decision 0051](decisions/0051-paste-images-from-the-items-menu.md).
  - **Paste Image…**, in the context menu of a task or its Items row, adds a new Image item to that task, named "Image", or "Image 2", "Image 3" and so on if the task already has an item with that name. No dashboard changes. The user drags the item from the sidebar onto a dashboard if wanted.
  - `Ctrl+V` replaces the image of the Image item in the focused cell, or of the item shown alone in a tab. This is an item edit, so it works on a locked dashboard, and in a root dashboard cell that shows the item. The old file is deleted when no item refers to it any more ([decision 0030](decisions/0030-deleting-image-files.md)). On any other cell, or without a focused cell, nothing changes, no dialog opens, and the status bar says "To add an image, use Paste Image… in a task's Items menu."
  - The new item is of the first type, by name, whose `pastedImageField` is set. In a build without such a type, such as one without the `image` module, nothing is pasted and the status bar says "This build cannot show images."
  - If the image cannot be saved, for example in the read-only sample data, nothing changes and the status bar says "The image could not be saved." For the read-only sample data this shows before any dialog opens.

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
