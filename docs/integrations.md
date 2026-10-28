# Integrations

This document describes how rraidash talks to GitLab, Jira and Claude, and how it starts terminals. Which connection an item uses is defined in the [data model](data-model.md#connection).

## GitLab

- **API:** REST v4 at `<baseUrl>/api/v4`. It works with gitlab.com and self-hosted GitLab.
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

Jira has two connection kinds, because Cloud and Data Center differ.

| | Jira Cloud (`jira-cloud`) | Jira Data Center (`jira-dc`) |
|---|---|---|
| API | REST v3 at `<baseUrl>/rest/api/3` | REST v2 at `<baseUrl>/rest/api/2` |
| Authentication | Basic, with the account email and an API token | `Authorization: Bearer <personal access token>` |
| Text format | ADF (JSON) | Wiki markup |

To avoid parsing either text format, every issue request adds `expand=renderedFields`. Jira then also returns the description and comments as HTML, which the summary shows in a `QTextBrowser`.

Main endpoints:

| Purpose | Endpoint |
|---|---|
| Issue | `GET /issue/:key?expand=renderedFields` |
| Search | `GET /search/jql` on Cloud, `GET /search` on Data Center, with the JQL query |
| Transitions | `GET /issue/:key/transitions`, `POST /issue/:key/transitions` |
| Comment | `POST /issue/:key/comment` |
| Current user | `GET /myself` |

Development information, meaning linked branches and merge requests, comes from a separate, less stable API. It is shown when available and left out quietly when not.

Jira priority is shown in the summary but never used for sorting. See [decision 0009](decisions/0009-priority-and-attention.md).

## Claude

- **Remote Control and cloud sessions** open as claude.ai pages in a [web view](web-views.md). There is no public API to list them or read their state, so the app shows no status for them.
- **Local Claude Code sessions** start in a terminal with `claude --resume <id>`, in the folder's local path. See [terminal launch](#terminal-launch).
- The app does not use the undocumented endpoints behind the claude.ai web interface, and it does not read Claude Code's stored login credentials. See [decision 0008](decisions/0008-claude-sessions.md).

If Anthropic publishes an API for sessions later, a new decision record will cover adding it.

## Polling and caching

The `StatusCache` owns all API data.

- **Cache key:** connection id plus request. Two groups that use the same server with different tokens never share data.
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

All requests are asynchronous and use one `QNetworkAccessManager`. At most four requests run at once for each connection.

## Errors

- A missing token shows "Token not set on this device" with a button to enter it.
- `401` or `403` marks the connection as failing, shows a banner and pauses polling for that connection until the token is changed.
- Network errors keep the last data and show its age, such as "Updated 12 min ago".

## Terminal launch

`TerminalLauncher` starts a command in a new terminal window, in a given folder. It is used by local Claude sessions and by the folder item.

| Platform | Default |
|---|---|
| Windows | `wt.exe -d <dir> <command>`. If Windows Terminal is missing: `cmd.exe /c start "" /d <dir> cmd /k <command>`. |
| macOS | `osascript` tells Terminal.app to run `cd <dir> && <command>`. |
| Linux | The first one found of: `x-terminal-emulator`, `gnome-terminal --working-directory=<dir> --`, `konsole --workdir <dir> -e`, `xterm -e`. |

On every platform a device setting can replace the default with a template, such as `alacritty --working-directory {dir} -e {command}`. Arguments are passed as a list, not through a shell, except where the platform requires a shell, as with `osascript`.

## Later: custom certificate authorities

Company servers are sometimes signed by an internal certificate authority. A connection could later hold a trusted certificate file for this case. It is not part of the first version.
