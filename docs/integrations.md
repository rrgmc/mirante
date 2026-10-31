# Integrations

This document describes how Mirante talks to GitLab, GitHub, Jira and Claude, and how it starts terminals. Which server an item talks to is defined in the [data model](data-model.md#servers-and-tokens): the GitLab, GitHub or Jira server in the module settings of the item's [profile](data-model.md#profile).

The GitLab, GitHub, Jira and Claude code lives in the `gitlab`, `github`, `jira` and `claude` [modules](modules.md). Polling, caching, errors and terminal launch are core services that all modules use.

## GitLab

- **API:** REST v4 at `<baseUrl>/api/v4`. It works with gitlab.com and self-hosted GitLab.
- **Server:** `gitlab.general.baseUrl` of the item's profile, `https://gitlab.com` by default. The token is entered on the same screen and kept in the keychain.
- **Authentication:** a personal access token in the `PRIVATE-TOKEN` header.
- **Token scope:** `api`, because the CI actions write data. A token with only `read_api` works too, but then those actions fail with a clear message.
- **Caching:** requests send `If-None-Match` with the last ETag. A `304 Not Modified` answer costs almost nothing and keeps cached data.
- **Paging:** lists follow the `Link` header, up to the item's `limit` field (100 rows by default).
- **Rate limits:** on `429 Too Many Requests` the client waits for the time given in `Retry-After` before trying again.

Main endpoints:

| Purpose | Endpoint |
|---|---|
| Merge request | `GET /projects/:id/merge_requests/:iid` |
| Approvals | `GET /projects/:id/merge_requests/:iid/approvals` |
| Merge request list | `GET /merge_requests`, `GET /projects/:id/merge_requests`, `GET /groups/:id/merge_requests` |
| Latest pipeline for a branch | `GET /projects/:id/pipelines/latest?ref=<branch>` |
| One pipeline | `GET /projects/:id/pipelines/:pid` |
| Pipelines of a project | `GET /projects/:id/pipelines`, also with `?ref=<branch>` or `?username=<me>` |
| Pipelines of a merge request | `GET /projects/:id/merge_requests/:iid/pipelines` |
| Jobs | `GET /projects/:id/pipelines/:pid/jobs` |
| Retry or cancel | `POST /projects/:id/pipelines/:pid/retry`, `.../cancel`, `POST /projects/:id/jobs/:jid/retry` |
| Play a manual job | `POST /projects/:id/jobs/:jid/play` |
| Trigger a pipeline | `POST /projects/:id/pipeline?ref=<branch>` |
| Issues | `GET /projects/:id/issues/:iid`, `GET /issues`, `GET /projects/:id/issues`, `GET /groups/:id/issues` |
| Latest issue notes | `GET /projects/:id/issues/:iid/notes?sort=desc&order_by=created_at` |
| Current user | `GET /user`, to resolve "me" in presets and attention |

Project and group paths such as `acme/backend` are URL-encoded and used as `:id`, as in `acme%2Fbackend`. The branch in `ref` is URL-encoded too. `GitLabClient` in the module has one method per endpoint; see the [module's README](../src/modules/gitlab/README.md#the-client).

What each item type asks for its status:

- **A merge request** (summary or page): the merge request, its approvals and the current user, at once. The current user is asked in every status job, and its ETag makes the repeat cheap. A summary whose head pipeline failed then asks for that pipeline's jobs. If that request fails, the status stands without the jobs.
- **An issue** (summary or page): the issue. A summary then asks for the latest 20 notes and leaves out system notes. If that request fails, the status stands without the notes.
- **A CI pipeline:** for a linked merge request, the merge request first, for its source branch. Then the latest pipeline of the branch. Then, at once, the latest 20 pipelines of the merge request or the branch and, for a fixed branch, its open merge request, for the link in the view. Then, at once, the jobs of each pipeline created within 10 minutes of the newest. A `404` for the latest pipeline means the branch has none, and is not an error. A failed request for the list, the open merge request or a sibling's jobs keeps the status of the latest. See [decision 0072](decisions/0072-pipelines-started-together.md).
- **A merge request or issue list:** the list of its scope, up to its `limit` (100 rows by default), most recently updated first. A merge request list with the `reviewRequested` preset asks for the current user first. See [decision 0067](decisions/0067-lists-report-status.md).
- **A CI pipeline list:** the pipelines of the project, up to its `limit`, newest first, all of them or those of a branch; those of a merge request; or, for "started by me", the current user and then that user's pipelines. GitLab's list answers leave out each pipeline's `user` and `started_at`, so the view's Author and Started columns usually show a dash. It makes no further requests for them.

The attention rules are in [item types](item-types.md#gitlab).

## GitHub

- **API:** REST, version `2022-11-28`. It works with github.com and GitHub Enterprise Server. See [decision 0102](decisions/0102-github-module.md).
- **Server:** `github.general.baseUrl` of the item's profile is the web address, `https://github.com` by default. The API address comes from it: `https://api.github.com/` for github.com, and `<baseUrl>/api/v3/` for any other host. Pages and pasted links use the web address. The token is entered on the same screen and kept in the keychain.
- **Authentication:** a personal access token in `Authorization: Bearer <token>`. Every request also sends `Accept: application/vnd.github+json`, `X-GitHub-Api-Version: 2022-11-28` and `User-Agent: Mirante`, because GitHub refuses a request without a user agent.
- **Token:** a fine-grained token needs read access to Metadata, Pull requests, Issues, Checks, Commit statuses and Actions, and write access to Actions for re-run and cancel. A classic token needs `repo`, or `public_repo` alone for public repositories. Without Actions write, the actions fail with a clear message.
- **Caching:** requests send `If-None-Match` with the last ETag, as for GitLab. GitHub does not count a `304` against the rate limit.
- **Paging:** lists follow the `Link` header, up to the item's `limit` field (100 rows by default). Search, workflow runs, jobs and check runs answer with an object that holds the rows under a key, such as `{"total_count": 3, "items": [...]}`. The module names that key in `GetOptions::arrayKey`, and the client joins the rows of every page under it.
- **Rate limits:** GitHub answers `403` or `429` with `x-ratelimit-remaining: 0` when the hourly limit is used up. The client then waits until `x-ratelimit-reset`, at least 1 second and at most 1 hour, and sends again. A `403` with `Retry-After`, for GitHub's secondary limits, waits that many seconds. Neither pauses the server. The search API has its own limit of 30 requests a minute, which the pull request and issue lists count against.

Main endpoints:

| Purpose | Endpoint |
|---|---|
| Current user | `GET /user`, to resolve "me" in attention, quick add and the run list |
| Repository | `GET /repos/:owner/:repo`, for its `default_branch` |
| Pull request | `GET /repos/:owner/:repo/pulls/:number` |
| Reviews | `GET /repos/:owner/:repo/pulls/:number/reviews` |
| Pull requests of a repository | `GET /repos/:owner/:repo/pulls?state=open&sort=updated&direction=desc` |
| Checks of a commit | `GET /repos/:owner/:repo/commits/:sha/check-runs`, `GET /repos/:owner/:repo/commits/:sha/status` |
| Issue | `GET /repos/:owner/:repo/issues/:number` |
| Issue comments | `GET /repos/:owner/:repo/issues/:number/comments?per_page=20&page=<p>` |
| Pull request and issue lists | `GET /search/issues?q=<query>&sort=updated&order=desc` |
| Workflow runs | `GET /repos/:owner/:repo/actions/runs`, `GET /repos/:owner/:repo/actions/workflows/:file/runs`, with `branch`, `actor` or `event` |
| Jobs of a run | `GET /repos/:owner/:repo/actions/runs/:id/jobs?filter=latest` |
| Re-run or cancel | `POST /repos/:owner/:repo/actions/runs/:id/rerun`, `.../rerun-failed-jobs`, `.../cancel`, `POST /repos/:owner/:repo/actions/jobs/:id/rerun` |

The owner and the repository's name are URL-encoded each on its own. A branch in a query and the search query are URL-encoded too. `GitHubClient` in the module has one method per endpoint; see the [module's README](../src/modules/github/README.md#the-client).

What each item type asks for its status:

- **A pull request** (summary or page): the pull request, its reviews and the current user, at once; then, at once, the check runs and the combined status of its head commit. If one of the last two fails, the checks are read from the other.
- **An issue** (summary or page): the issue. A summary then asks for the last page of comments, 20 per page, and the page before it when the last holds fewer than 5. GitHub gives comments oldest first and cannot sort them. If that request fails, the status stands without the comments.
- **An Actions run:** for a linked pull request, the pull request first, for its head branch and commit. For an empty branch, the repository, for its default branch. Then the latest 20 runs of the branch, of one workflow if the item names one. Then, at once, the jobs of each run of the newest commit, or of the pull request's head commit. A failed request for a sibling's jobs keeps the status of the lead run.
- **A pull request or issue list:** one search, up to its `limit`, most recently updated first. The query holds `is:pr` or `is:issue`, `is:open`, the scope, the preset and the filter. See [decision 0102](decisions/0102-github-module.md).
- **An Actions run list:** the runs of the repository, up to its `limit`, newest first: all of them; those of a branch; those of a pull request's head branch, after the pull request; or, for "started by me", the current user and then that user's runs.

The attention rules are in [item types](item-types.md#github).

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

To avoid parsing either text format, every issue request adds `expand=renderedFields`. Jira then also returns the description and comments as HTML, which the summary shows in a `QTextBrowser`.

Main endpoints, below the API root:

| Purpose | Endpoint |
|---|---|
| Issue | `GET /issue/:key?expand=renderedFields`, with `fields` to limit the answer, and `names` added to `expand` where the sprint is needed |
| Search | `GET /search/jql` on Cloud, `GET /search` on Data Center, with the JQL query, up to the item's `limit` rows, 100 by default |
| Fields | `GET /field`, the id and name of each field, where a list has extra fields |
| Current user | `GET /myself` |

What each item type asks for its status:

- **An issue page:** the issue, with only `summary`, `status`, `assignee`, `priority`, `issuetype` and `updated`.
- **An issue summary:** the issue, with all fields and `expand=renderedFields,names`. The sprint is the field whose entry in `names` is "Sprint", because its id differs per site. Then a search for its children, with `summary` and `status`: `parent = "<key>"` on Cloud, and `"Epic Link" = "<key>" OR parent = "<key>"` on Data Center. Cloud's search gives no total, so the children are counted by fetching them, up to 500. If only this search fails, as on a site without the `Epic Link` field, the status stands without the children.
- **A list:** one search, with `summary`, `status`, `assignee` and `updated`, up to the item's `limit` rows. With extra fields, `GET /field` comes first, since a search's `fields` takes only ids and they differ per site. The search then also asks for those fields and for `expand=names`. If `GET /field` fails, the search goes ahead without them. A project preset without a project key, or an empty query, sends nothing. See [decision 0067](decisions/0067-lists-report-status.md).

See [item types](item-types.md#jira-issue-summary).

Development information, meaning linked branches and merge requests, comes from a separate, less stable API. M6 does not ask for it, so it does not appear.

Jira priority is shown in the summary but never used for sorting. See [decision 0009](decisions/0009-priority-and-attention.md).

## Claude

- **Remote Control and cloud sessions** open as claude.ai pages in a [web view](web-views.md). There is no public API to list them or read their state, so the app shows no status for them.
- **Local Claude Code sessions** have no item. M7 had one that ran `claude --resume <id>` in a terminal; [decision 0084](decisions/0084-no-local-claude-sessions.md) removed it.
- The app does not use the undocumented endpoints behind the claude.ai web interface, and it does not read Claude Code's stored login credentials. See [decision 0008](decisions/0008-claude-sessions.md).

If Anthropic publishes an API for sessions later, a new decision record will cover adding it.

## Polling and caching

The `StatusCache` owns all API data. It keeps one status per item, in memory only: after a start, cells show no status until the first fetch returns.

- **Cache key:** profile id, module id and request. Two profiles that use the same server with different tokens never share data.
- **What it polls:** the items of types that report status, if they are
  - in visible cells, counted per placement;
  - in tasks opened in a tab or a slot in the last 30 minutes;
  - in active Urgent and High tasks, so that Focus stays current;
  - [watched](ui.md#watched-items) in active tasks, so that the tray and notifications stay current.

  Items in the trash, items of a module that is not in the build, and items of types without status are never fetched.
- **Interval:** a [device setting](sync.md#device-settings), 2 minutes by default, from 30 seconds to one hour. A timer checks every 15 seconds and fetches each due item whose last fetch is older than the interval, so a new visible item does not wait for a whole interval.
- **At once:** an item is fetched at once when it becomes visible or its task is opened and its status is older than the interval, when its cell's refresh button is pressed, and when its type, configuration or profile changes, or its module's settings in the profile or the project change.
- **One job per item.** A fetch for an item already being fetched is skipped. A fetch that does not end in 90 seconds is dropped with an error. A fetch started before the item's configuration changed is dropped too, and its answer ignored.
- **Errors keep data.** A fetch that fails keeps the last data, summary, attention and update time, and sets only the state and the message. Cells then show the data's age. An error is fetched again after the interval, like any status.
- **Removal.** Removing an item empties its entry and removes its [seen attention keys](sync.md#device-settings). Opening another workspace empties every entry.
- **When the app is hidden in the tray:** polling continues for the same items, at the same interval. Cells stay in their placements while the window is hidden.
- **Consumers:**
  - native summaries and lists;
  - [attention badges](ui.md#attention) and the tray count;
  - [built-in panels](web-views.md#built-in-panels), through the bridge.

Modules poll through the cache, never around it. A view asks the server directly only for what a user's action or a visible list needs, such as the pipeline and approvals of a merge request list's rows. See [decision 0068](decisions/0068-views-read-what-the-user-asked-for.md). All requests are asynchronous and use one `QNetworkAccessManager`. At most four requests run at once for each server of a profile.

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

`Launcher` in `src/platform/` starts a program in a new terminal window, in a given folder, and starts programs such as an editor on their own. It is used by the folder item, and stays in `ItemContext` for other modules that need it. Modules pass a program and a list of arguments, never one command string, as [decision 0082](decisions/0082-commands-as-argument-lists.md) says. `Launcher` quotes them for each layer they pass through.

| Platform | Default, in the order tried |
|---|---|
| Windows | `wt.exe -d <dir> <program> <arguments>`. If Windows Terminal is missing: `cmd.exe /c start "" /d <dir> cmd /k <program> <arguments>`. |
| macOS | `osascript` tells Terminal.app to `do script "cd <dir> && <program> <arguments>"`. |
| Linux | The first one found of: `x-terminal-emulator -e`, `gnome-terminal --working-directory=<dir> --`, `konsole --workdir <dir> -e`, `xterm -e`. `x-terminal-emulator` and `xterm` have no working folder option, so they are started in the folder. |

With no program, as for the folder item's **Terminal** action, the terminal opens alone in the folder.

**Quoting:**

- **Windows Terminal** reads `;` as the start of a new tab, so each `;` in the folder and the arguments is passed as `\;`.
- **`cmd.exe`** has its own rules. Each argument is first quoted for the program, as the Microsoft C runtime reads it, then each character that `cmd` treats as special (`( ) % ! ^ " < > & |`) gets a `^` before it, and so do the separators space, `;` and `,`. A quote escaped this way does not start a quoted region, so without this `cmd` would split a path such as `...\Microsoft VS Code\bin\code.cmd` at its spaces. The fallback terminal passes through two `cmd.exe`, the one that runs `start` and the one that `start` opens, so the command is escaped twice.
- **macOS:** each word is quoted for the shell in single quotes, unless it holds only safe characters. The whole line is then escaped for the AppleScript string, with `\\` for `\` and `\"` for `"`.
- On Windows, `launchTerminal()` finds the program on the `PATH` first, and passes its full path, because the terminal does not try `.cmd` files. A program installed with npm, for example, is a `.cmd` file.

**Programs on their own,** such as the editor, are found on the `PATH` unless they have a path, and started detached in the folder. On Windows the `PATH` lookup tries the suffixes in `PATHEXT`, so `code` finds `code.cmd`. A `.cmd` or `.bat` file runs as `cmd.exe /c <file> <arguments>`, quoted and escaped for `cmd` as above, without a console window of its own.

**Terminal command:** a device setting replaces the default on every platform with a template. It is set in the [settings window](ui.md#settings) and kept in the [device settings](sync.md#device-settings), so each device has its own. An example is `alacritty --working-directory {dir} -e {command}`.

- The template is split into arguments as a shell would: spaces separate them, and double quotes keep spaces in one. A backslash is an ordinary character, so Windows paths need no escaping.
- `{dir}` is replaced by the folder inside any argument.
- An argument that is exactly `{command}` becomes the program and its arguments, each as a separate argument. With no program, it becomes nothing.
- The terminal is also started in the folder, for a template without `{dir}`.
- An empty template means the default.

A template that needs one string, such as `sh -c`, must put `{command}` where a list of arguments fits.

When no terminal is found, the user sees "No terminal was found.", or "The terminal <program> was not found." for a template. A program that is not found gives "<program> was not found."

## Later: custom certificate authorities

Company servers are sometimes signed by an internal certificate authority. A module's settings could later hold a trusted certificate file for this case. It is not part of the first version.
