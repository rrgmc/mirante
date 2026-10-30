# Integrations

This document describes how Mirante talks to GitLab, Jira and Claude, and how it starts terminals. Which server an item talks to is defined in the [data model](data-model.md#servers-and-tokens): the GitLab or Jira server in the module settings of the item's [profile](data-model.md#profile).

The GitLab, Jira and Claude code lives in the `gitlab`, `jira` and `claude` [modules](modules.md). Polling, caching, errors and terminal launch are core services that all modules use.

## GitLab

- **API:** REST v4 at `<baseUrl>/api/v4`. It works with gitlab.com and self-hosted GitLab.
- **Server:** `gitlab.general.baseUrl` of the item's profile, `https://gitlab.com` by default. The token is entered on the same screen and kept in the keychain.
- **Authentication:** a personal access token in the `PRIVATE-TOKEN` header.
- **Token scope:** `api`, because the CI actions write data. A token with only `read_api` works too, but then those actions fail with a clear message.
- **Caching:** requests send `If-None-Match` with the last ETag. A `304 Not Modified` answer costs almost nothing and keeps cached data.
- **Paging:** lists follow the `Link` header, up to a limit per item (100 rows by default).
- **Rate limits:** on `429 Too Many Requests` the client waits for the time given in `Retry-After` before trying again.

Main endpoints:

| Purpose | Endpoint |
|---|---|
| Merge request | `GET /projects/:id/merge_requests/:iid` |
| Approvals | `GET /projects/:id/merge_requests/:iid/approvals` |
| Merge request list | `GET /merge_requests`, `GET /projects/:id/merge_requests`, `GET /groups/:id/merge_requests` |
| Latest pipeline for a branch | `GET /projects/:id/pipelines/latest?ref=<branch>` |
| One pipeline | `GET /projects/:id/pipelines/:pid` |
| Jobs | `GET /projects/:id/pipelines/:pid/jobs` |
| Retry or cancel | `POST /projects/:id/pipelines/:pid/retry`, `.../cancel`, `POST /projects/:id/jobs/:jid/retry` |
| Issues | `GET /projects/:id/issues/:iid`, `GET /issues`, `GET /projects/:id/issues`, `GET /groups/:id/issues` |
| Latest issue notes | `GET /projects/:id/issues/:iid/notes?sort=desc&order_by=created_at` |
| Current user | `GET /user`, to resolve "me" in presets and attention |

Project and group paths such as `acme/backend` are URL-encoded and used as `:id`, as in `acme%2Fbackend`. The branch in `ref` is URL-encoded too. `GitLabClient` in the module has one method per endpoint; see the [module's README](../src/modules/gitlab/README.md#the-client).

What each item type asks for its status:

- **A merge request** (summary or page): the merge request, its approvals and the current user, at once. The current user is asked in every status job, and its ETag makes the repeat cheap. A summary whose head pipeline failed then asks for that pipeline's jobs. If that request fails, the status stands without the jobs.
- **An issue** (summary or page): the issue. A summary then asks for the latest 20 notes and leaves out system notes. If that request fails, the status stands without the notes.
- **A CI pipeline:** for a linked merge request, the merge request first, for its source branch. Then the latest pipeline of the branch, and that pipeline's jobs. A `404` for the latest pipeline means the branch has none, and is not an error.
- **A list:** the list of its scope, up to 100 rows, most recently updated first. A merge request list with the `reviewRequested` preset asks for the current user first. See [decision 0067](decisions/0067-lists-report-status.md).

The attention rules are in [item types](item-types.md#gitlab).

## Jira

Jira Cloud and Data Center differ. The server is `jira.general.baseUrl` of the item's profile, with its token on the same screen. The **Deployment** setting on that screen, `jira.general.deployment`, tells them apart ([decision 0060](decisions/0060-jira-cloud-or-data-center-by-setting.md)):

- `auto`, the default, means Jira Cloud when the base URL's host ends in `.atlassian.net` or `.jira.com`, and Jira Data Center otherwise;
- `cloud` and `dc` choose one, such as for a Jira Cloud site on its own domain.

| | Jira Cloud | Jira Data Center |
|---|---|---|
| API | REST v3 at `<baseUrl>/rest/api/3` | REST v2 at `<baseUrl>/rest/api/2` |
| Authentication | `Authorization: Basic`, with `jira.general.email` and an API token as `email:token` | `Authorization: Bearer <personal access token>` |
| Text format | ADF (JSON) | Wiki markup |
| Search paging | `nextPageToken`, until `isLast` | `startAt`, until `total` |

On Jira Cloud without an email, every request fails before it is sent, and the item shows "Set the email for Jira Cloud in the profile's Jira settings."

To avoid parsing either text format, every issue request adds `expand=renderedFields`. Jira then also returns the description and comments as HTML, which the summary shows in a `QTextBrowser`. A comment is written as plain text: the module turns it into an ADF document on Cloud, with a paragraph for each block between blank lines, and sends it as it is on Data Center.

Main endpoints, below the API root:

| Purpose | Endpoint |
|---|---|
| Issue | `GET /issue/:key?expand=renderedFields`, with `fields` to limit the answer, and `names` added to `expand` where the sprint is needed |
| Search | `GET /search/jql` on Cloud, `GET /search` on Data Center, with the JQL query, up to 100 rows |
| Transitions | `GET /issue/:key/transitions`, `POST /issue/:key/transitions` |
| Comment | `POST /issue/:key/comment` |
| Current user | `GET /myself` |

What each item type asks for its status:

- **An issue page:** the issue, with only `summary`, `status`, `assignee`, `priority`, `issuetype` and `updated`.
- **An issue summary:** the issue, with all fields and `expand=renderedFields,names`. The sprint is the field whose entry in `names` is "Sprint", because its id differs per site.
- **A list:** one search, with `summary`, `status`, `assignee` and `updated`, up to 100 rows. A project preset without a project key, or an empty query, sends nothing. See [decision 0067](decisions/0067-lists-report-status.md).

See [item types](item-types.md#jira-issue-summary).

A summary or a preview asks for the transitions when the Change status menu opens. A preview also reads its issue with the summary's request. See [decision 0068](decisions/0068-views-read-what-the-user-asked-for.md).

Development information, meaning linked branches and merge requests, comes from a separate, less stable API. M6 does not ask for it, so it does not appear.

Jira priority is shown in the summary but never used for sorting. See [decision 0009](decisions/0009-priority-and-attention.md).

## Claude

- **Remote Control and cloud sessions** open as claude.ai pages in a [web view](web-views.md). There is no public API to list them or read their state, so the app shows no status for them.
- **Local Claude Code sessions** start in a terminal with `claude --resume <id>`, in the folder's local path. See [terminal launch](#terminal-launch).
- The app does not use the undocumented endpoints behind the claude.ai web interface, and it does not read Claude Code's stored login credentials. See [decision 0008](decisions/0008-claude-sessions.md).

If Anthropic publishes an API for sessions later, a new decision record will cover adding it.

## Polling and caching

The `StatusCache` owns all API data. It keeps one status per item, in memory only: after a start, cells show no status until the first fetch returns.

- **Cache key:** profile id, module id and request. Two profiles that use the same server with different tokens never share data.
- **What it polls:** the items of types that report status, if they are
  - in visible cells, counted per placement;
  - in tasks opened in a tab or a slot in the last 30 minutes;
  - in active Urgent and High tasks, so that Focus and the tray stay current.

  Items in the trash, items of a module that is not in the build, and items of types without status are never fetched.
- **Interval:** a [device setting](sync.md#device-settings), 2 minutes by default, from 30 seconds to one hour. A timer checks every 15 seconds and fetches each due item whose last fetch is older than the interval, so a new visible item does not wait for a whole interval.
- **At once:** an item is fetched at once when it becomes visible or its task is opened and its status is older than the interval, when its cell's refresh button is pressed, and when its type, configuration or profile changes, or its module's settings in the profile or the project change.
- **One job per item.** A fetch for an item already being fetched is skipped. A fetch that does not end in 90 seconds is dropped with an error. A fetch started before the item's configuration changed is dropped too, and its answer ignored.
- **Errors keep data.** A fetch that fails keeps the last data, summary, attention and update time, and sets only the state and the message. Cells then show the data's age. An error is fetched again after the interval, like any status.
- **Removal.** Removing an item empties its entry and removes its [seen attention keys](sync.md#device-settings). Opening another workspace empties every entry.
- **When the app is hidden in the tray:** polling continues for pipelines and attention only, at the same interval.
- **Consumers:**
  - native summaries and lists;
  - [attention badges](ui.md#attention) and the tray count;
  - [built-in panels](web-views.md#built-in-panels), through the bridge.

Modules poll through the cache, never around it. A view asks the server directly only for what a user's action or a visible list needs, such as a list row's preview. See [decision 0068](decisions/0068-views-read-what-the-user-asked-for.md). All requests are asynchronous and use one `QNetworkAccessManager`. At most four requests run at once for each server of a profile.

### Attention

`Attention` turns the statuses into the counts of badges. See [decision 0061](decisions/0061-attention-from-status-of-any-rendering.md).

- An item **needs attention** when its status has a reason that counts: a state, such as "Pipeline failed", or an event, marked `clearsOnView`, whose key the item's record of seen keys does not hold.
- **The first status with data** records the keys of its events as seen, so what was already true when the item was added does not count. A first status without data, such as "Token not set", records nothing.
- **Seeing an item** replaces its record with the keys of its current events. Keys no longer reported drop out, so the record stays small.
- A workspace without an id has nowhere to keep records, so events do not count there. States still do.
- A **task** counts its items that need attention. Tasks that are not active, and tasks in the trash, count zero. A **project** sums its tasks, and a **group** its projects; a project in the trash counts zero.

## Errors

- A missing token shows "Token not set on this device" with a button to enter it. Nothing is sent.
- A `401` on any request, or a `403` on a `GET`, marks the profile's server as failing. The main window's warning bar shows a line for it, and the server's requests return without being sent until its token or its settings screen changes.
- A `403` on a `POST` or a `PUT` is a missing permission for that one action, such as a `read_api` token retrying a pipeline. Only that action fails; the server keeps working.
- A `429` waits for `Retry-After` seconds, or 30 without one, and sends again, at most three times. The server's other requests wait too.
- A request that gets no answer in 30 seconds fails as a network error.
- Network errors keep the last data and show its age, such as "Updated 12 min ago".
- Status data is kept in memory only, not on disk. After a start, cells show no status until the first poll returns. A disk cache can come later if the wait is a problem.

Each server client keeps the `ETag` and body of the 500 URLs it used last, and sends `If-None-Match`, so an unchanged answer costs a `304`. With Link paging, it follows `rel="next"` only to the same server, so the token is never sent elsewhere.

## Terminal launch

`TerminalLauncher` starts a command in a new terminal window, in a given folder. It is used by local Claude sessions and by the folder item.

| Platform | Default |
|---|---|
| Windows | `wt.exe -d <dir> <command>`. If Windows Terminal is missing: `cmd.exe /c start "" /d <dir> cmd /k <command>`. |
| macOS | `osascript` tells Terminal.app to run `cd <dir> && <command>`. |
| Linux | The first one found of: `x-terminal-emulator`, `gnome-terminal --working-directory=<dir> --`, `konsole --workdir <dir> -e`, `xterm -e`. |

On every platform a device setting can replace the default with a template, such as `alacritty --working-directory {dir} -e {command}`. Arguments are passed as a list, not through a shell, except where the platform requires a shell, as with `osascript`.

## Later: custom certificate authorities

Company servers are sometimes signed by an internal certificate authority. A module's settings could later hold a trusted certificate file for this case. It is not part of the first version.
