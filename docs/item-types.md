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
| `fields` | The configuration fields, with type, default and validation |
| `rendering` | `native` (a Qt widget) or `web` (a [web view](web-views.md)) |
| `connectionKind` | The kind of [connection](data-model.md#connection) it needs, if any |
| `status` | Optional: how to fetch status and attention from the [status cache](integrations.md#polling-and-caching) |
| `actions` | Optional: the light actions it offers |
| `fromUrl` | Optional: recognises a pasted URL and returns a configuration |

New types are added by registering a class in a module. No core code changes. See [adding a module](modules.md#adding-a-module).

When a field has a project default, the type uses it when the field is empty. For example, an empty project path in an MR List uses the project's `gitlab.project` setting. See [module settings](data-model.md#project).

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

- **Configuration:** project path and merge request number. Pasting a merge request URL fills both.
- **Shows:**
  - title, state, draft flag;
  - source and target branches;
  - author, assignees, reviewers, labels;
  - latest pipeline status, with the failed jobs listed;
  - approvals: the count required and who has approved.
- **Actions:** approve, retry pipeline, add comment.
- **Attention:** pipeline failed, review requested from you, approved, has conflicts.

### GitLab MR (page)

`gitlab.mr.page`, web. The merge request's web page, using the group's [profile](web-views.md#profiles).

### GitLab MR List

`gitlab.mr.list`, native.

- **Configuration:**
  - scope: one project, one GitLab group, or all projects visible to you;
  - preset: "Created by me", "Review requested from me", "Assigned to me" or "All open";
  - optional custom filter parameters, passed to the GitLab merge requests API, such as `labels=bug&target_branch=main`.
- **Shows:** a compact, sortable table with columns for reference, title, author, pipeline status, approvals, and the date of the last update.
- **Row click:** shows the merge request summary in a preview area below the table. A row menu offers "Add to task as item".

### GitLab CI Pipeline

`gitlab.pipeline`, native.

- **Configuration:**
  - project path;
  - branch, with three choices: a fixed branch, "current branch of the main folder", or "branch of a linked merge request".
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
- **The list presets are:** "Assigned to me", "Created by me" and "All open". Custom filters work the same way as for merge requests.
- **Actions:** add comment, close and reopen.

## Jira

Module `jira`. Jira types need a `jira.cloud` or `jira.dc` connection.

### Jira Issue (summary)

`jira.issue.summary`, native.

- **Configuration:** issue key. Pasting an issue URL fills it.
- **Shows:**
  - key, summary, status, type, priority, assignee, reporter, sprint;
  - the description, rendered from the HTML in `renderedFields`;
  - the latest comments;
  - sub-tasks and linked issues;
  - development information: linked branches and merge requests, where the Jira instance provides it.
- **Actions:** change status, using the transitions Jira allows for the issue; add comment.
- **Attention:** the status changed since the user last viewed the item.

### Jira Issue (page)

`jira.issue.page`, web. The issue's web page.

### Jira List

`jira.list`, native.

- **Configuration:** a JQL query (Jira's query language), or one of these presets:
  - "My open issues";
  - "Current sprint of this project", using the project's `jira.projectKey` setting;
  - "Recently updated in this project".
- **Shows:** a sortable table with columns for key, summary, status, assignee and the date of the last update.
- **Row click:** shows the issue summary in a preview area. A row menu offers "Add to task as item".

## Claude

Module `claude`.

### Claude session (web)

`claude.session.web`, web.

- **Configuration:** a claude.ai URL, such as a Remote Control or cloud session link.
- **Shows:** the claude.ai page, using the group's profile.

There is no public API for these sessions, so the app has no status or summary for them. See [decision 0008](decisions/0008-claude-sessions.md).

### Claude session (local)

`claude.session.local`, native.

- **Configuration:**
  - session id;
  - folder, one of the project's folders, the main folder by default;
  - a note about which device the session lives on.
- **Shows:** the session id, folder and device, plus a "Resume in terminal" button.
- **Action:** runs `claude --resume <id>` in a terminal in the folder's local path. See [terminal launch](integrations.md#terminal-launch).

Local sessions exist on one device only. On other devices the panel shows the device name and disables the button.

## General

### Web page

`web.page`, web, module `web`.

- **Configuration:** a URL.
- **Shows:** the page, using the group's profile, or the shared profile if the host is overridden.

### Notes

`notes`, native, module `notes`.

- **Configuration:** none.
- **Shows:** a Markdown editor with a preview. The text is stored in the task file, so it syncs.

### Folder / terminal

`folder`, native, module `folder`.

- **Configuration:** one of the project's folders.
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
