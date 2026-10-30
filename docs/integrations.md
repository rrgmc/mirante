# Integrations

This document describes how Mirante talks to GitLab, Jira and Claude, and how it starts terminals. Which server an item talks to is defined in the [data model](data-model.md#servers-and-tokens): the GitLab or Jira server in the module settings of the item's [profile](data-model.md#profile).

The GitLab, Jira and Claude code lives in the `gitlab`, `jira` and `claude` [modules](modules.md). Polling, caching, errors and terminal launch are core services that all modules use.

## GitLab

- **API:** REST v4 at `<baseUrl>/api/v4`. It works with gitlab.com and self-hosted GitLab.
- **Server:** `gitlab.general.baseUrl` of the item's profile, `https://gitlab.com` by default. The token is entered on the same screen and kept in the keychain.
- **Authentication:** a personal access token in the `PRIVATE-TOKEN` header.
- **Token scope:** `api`, because the light actions write data. A token with only `read_api` works too, but then the actions fail with a clear message.
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
| Jobs | `GET /projects/:id/pipelines/:pid/jobs` |
| Retry or cancel | `POST /projects/:id/pipelines/:pid/retry`, `.../cancel`, `POST /projects/:id/jobs/:jid/retry` |
| Approve | `POST /projects/:id/merge_requests/:iid/approve` |
| Comment | `POST /projects/:id/merge_requests/:iid/notes`, `POST /projects/:id/issues/:iid/notes` |
| Issues | `GET /projects/:id/issues/:iid`, `GET /issues` |
| Current user | `GET /user`, to resolve "me" in presets |

Project paths such as `acme/backend` are URL-encoded and used as `:id`.

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
| Issue | `GET /issue/:key?expand=renderedFields`, with `fields` to limit the answer |
| Search | `GET /search/jql` on Cloud, `GET /search` on Data Center, with the JQL query, up to 100 rows |
| Transitions | `GET /issue/:key/transitions`, `POST /issue/:key/transitions` |
| Comment | `POST /issue/:key/comment` |
| Current user | `GET /myself` |

The status of an issue item asks for the issue with only `summary`, `status`, `assignee`, `priority`, `issuetype` and `updated`. See [item types](item-types.md#jira-issue-summary).

Development information, meaning linked branches and merge requests, comes from a separate, less stable API. It is shown when available and left out quietly when not.

Jira priority is shown in the summary but never used for sorting. See [decision 0009](decisions/0009-priority-and-attention.md).

## Claude

- **Remote Control and cloud sessions** open as claude.ai pages in a [web view](web-views.md). There is no public API to list them or read their state, so the app shows no status for them.
- **Local Claude Code sessions** start in a terminal with `claude --resume <id>`, in the folder's local path. See [terminal launch](#terminal-launch).
- The app does not use the undocumented endpoints behind the claude.ai web interface, and it does not read Claude Code's stored login credentials. See [decision 0008](decisions/0008-claude-sessions.md).

If Anthropic publishes an API for sessions later, a new decision record will cover adding it.

## Polling and caching

The `StatusCache` owns all API data.

- **Cache key:** profile id, module id and request. Two profiles that use the same server with different tokens never share data.
- **What it polls:**
  - items in visible cells;
  - items in tasks opened in the last 30 minutes;
  - all items of active Urgent and High tasks, so that Focus and the tray stay current.
- **Interval:** a device setting, 2 minutes by default. A refresh button in each cell header fetches at once.
- **When the app is hidden in the tray:** polling continues for pipelines and attention only, at the same interval.
- **Consumers:**
  - native summaries and lists;
  - [attention badges](ui.md#attention) and the tray count;
  - [built-in panels](web-views.md#built-in-panels), through the bridge.

Modules fetch through the cache, never around it. All requests are asynchronous and use one `QNetworkAccessManager`. At most four requests run at once for each server of a profile.

## Errors

- A missing token shows "Token not set on this device" with a button to enter it. Nothing is sent.
- A `401` on any request, or a `403` on a `GET`, marks the profile's server as failing. The main window shows a banner, and the server's requests return without being sent until its token or its settings screen changes.
- A `403` on a `POST` is a missing permission for that one action, such as a `read_api` token approving a merge request. Only that action fails; the server keeps working.
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
