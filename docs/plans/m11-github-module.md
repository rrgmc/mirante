# M11 code plan: GitHub module

This plan implements [milestone M11](../roadmap.md#m11-github-module). After M11, a `github` module offers for GitHub and GitHub Enterprise Server what the `gitlab` module offers for GitLab: pull request, Actions run and issue items, as summaries, pages and lists, with status, attention, pasted links and quick add.

The GitLab module is the model. Its [README](../../src/modules/gitlab/README.md) describes each part in detail, and this plan says where GitHub differs. A task that meets a question this plan leaves open does what the GitLab module does.

## Decisions

- [Decision 0102](../decisions/0102-github-module.md), proposed: a GitHub module shaped like the GitLab module, with lists from the search API, one web base URL, `GetOptions::arrayKey`, rate limits that wait on `403`, run groups by commit, and reviews instead of approvals.

Smaller choices made here:

- **Ids follow GitHub's words.** A repository is `repository` (`owner/repo`), a pull request or issue number is `number`, and the CI item is a run, not a pipeline.
- **An empty branch is the default branch.** GitLab's empty branch means `master`. GitHub repositories have a `default_branch`, which a status job reads from `GET repos/<repository>` when `branch` is empty. Its ETag makes the repeat cheap.
- **No "trigger".** GitHub starts a run by hand only for a workflow with `workflow_dispatch`, often with inputs. `github.run` re-runs and cancels, but does not start runs. See [ideas for later](#ideas-for-later).
- **Translations at the end.** Every task writes its texts with `QCoreApplication::translate("github", …)`, but does not edit `translations/`. The orchestrator runs `task translations` and translates the `github` context once, after all tasks are merged, so parallel branches never conflict in the `.ts` file. Until then `task check` reports the untranslated texts; tasks run the tests, not `task check`.

## Changes to the architecture

- **`GetOptions::arrayKey`** in `ApiClient.h`. See [Part 1](#part-1-the-core).
- **Rate-limit headers on `403`** in `ServerClient`. See [Part 1](#part-1-the-core).
- **A new module,** `src/modules/github/`, enabled by default. No other module changes. The `no-modules` preset leaves it out like the others.

## Part 1: the core

```cpp
// ApiClient.h
struct GetOptions
{
    // Follow the Link header's "next" page and join the JSON arrays, up to
    // maxRows rows. GitLab and GitHub lists use it. Jira pages itself.
    bool followLinks = false;
    int maxRows = 100;
    // With followLinks, for an answer that is an object holding its rows,
    // such as GitHub's {"total_count": 3, "items": [...]}: the key of the
    // array to join. The answer is the first page's object, with all the
    // rows under this key. Empty: the answer itself is the array.
    QString arrayKey;
};
```

`ServerClient`:

- **`arrayKey`.** With `followLinks` and an `arrayKey`, a page whose JSON is an object with an array under that key joins that array, as an array answer does now. The joined answer is the first page's object, with the key replaced by the joined rows, cut to `maxRows`. An object without the key ends the paging and answers as it is. A `304` keeps working as it does for arrays: each page's cached body takes the place of the page.
- **Rate limits.** A `403` or `429` whose headers hold `x-ratelimit-remaining: 0` waits until `x-ratelimit-reset` (Unix seconds), at least 1 second and at most 1 hour, then sends again, holding the queue as a `429` does now. A `403` with `Retry-After` waits that many seconds, the same way. Neither is `Refused`, and neither pauses the server. While it waits, the requests on the way stay queued, as for a `429`; the reply message, if the wait is given up, is "Rate limited (403)" or the `429`'s current one.
- **Tests** in the core's `ServerClient` tests: an object answer joined over three pages, cut at `maxRows`; an object without the key; a `304` page; a `403` with `x-ratelimit-remaining: 0` that waits and succeeds; a `403` with `Retry-After`; a plain `403` on a `GET` that is still `Refused`.

## Part 2: the module, server, settings, links and pages

**Folder.** `src/modules/github/`, in the namespace `mirante::modules::github`, with `GitHubModule.cpp`, `icons/github.svg` (the GitHub mark, single colour, `currentColor` where the GitLab icon does the same), `README.md` and `tests/`. Files are named as in `gitlab`, with `GitHub` for `GitLab`: `GitHubClient`, `GitHubSettings`, `GitHubUrls`, `GitHubStatus`, `GitHubLists`, `GitHubQuickAdd`, `GitHubViewSupport`, `GitHubListSupport`, and one file per view.

**Identity.**

| Property | Value |
|---|---|
| Id | `github` |
| Name | "GitHub" |
| Description | "Pull requests, Actions runs and issues from GitHub or GitHub Enterprise Server. Each profile sets its server and token in the module's General Configuration." |
| UUID | `6be6a6eb-abdb-41f6-addb-14dd5f6cb935` |
| Version, author | `0.1.0`, Rangel Reale |
| Homepage | `https://github.com/rrgmc/mirante/tree/master/src/modules/github` |

**Item types.** Every type has `hasStatus()` true and implements `fromUrl()`. The page types are web, the others native.

| Id | Name | Short name | Description |
|---|---|---|---|
| `github.pr.summary` | GitHub PR (summary) | PR | A summary of one pull request: its state, branches, checks, reviews and people. |
| `github.pr.page` | GitHub PR (page) | PR | The web page of one pull request, on the GitHub server of the item's profile. |
| `github.pr.list` | GitHub PR List | PRs | A table of pull requests in a repository, an owner or everywhere, such as those that wait for your review. |
| `github.run` | GitHub Actions Run | Run | The Actions runs of a branch's newest commit, one per workflow, with their jobs. The branch is a fixed one, the main folder's current branch or a pull request's branch. |
| `github.run.list` | GitHub Actions Run List | Runs | A table of a repository's Actions runs: all of them, those you started, or those of a branch or a pull request. |
| `github.issue.summary` | GitHub Issue (summary) | Issue | A summary of one issue: its state, assignees, labels, milestone and latest comments. |
| `github.issue.page` | GitHub Issue (page) | Issue | The web page of one issue, on the GitHub server of the item's profile. |
| `github.issue.list` | GitHub Issue List | Issues | A table of issues in a repository, an owner or everywhere, such as those assigned to you. |

The short names follow those of the GitLab types: a page and its summary share one.

**Configuration.**

| Key | Type | Used by | Description |
|---|---|---|---|
| `repository` | text | all | `owner/repo`. Empty uses the project's `github.repository` setting. The form offers the repositories of General Configuration in the item's profile, as GitLab's `project` does. |
| `number` | integer | PR and issue summary and page; run; run list | The pull request or issue number. Required for the summaries and pages; for the run and run list it names the linked pull request. |
| `scope` | choice | PR and issue lists | `repository` (the default), `owner` (all repositories of a user or organisation) or `all`. |
| `owner` | text | PR and issue lists | The user or organisation, used when `scope` is `owner`. |
| `preset` | choice | PR and issue lists | PR List: `createdByMe` (the default), `reviewRequested`, `assignedToMe` or `allOpen`. Issue List: `assignedToMe` (the default), `createdByMe` or `allOpen`. |
| `filter` | text | PR and issue lists | Extra search qualifiers, such as `label:bug base:main`. |
| `branchSource` | choice | run | `fixed` (the default), `mainFolder` or `pullRequest` (the head branch of pull request `number`). |
| `branch` | text | run; run list | The branch for `fixed` and `branch`. Empty means the repository's default branch, and the placeholder says so. |
| `workflow` | text | run; run list | A workflow file name, such as `ci.yml`. Empty means all workflows. |
| `source` | choice | run list | `all` (the default), `mine`, `branch` or `pullRequest`. |
| `limit`, `pageSize` | integer | lists | As in GitLab: 100 and 50 by default, also when absent or below 1. |
| `showChecks`, `showReviews`, `showPeople`, `showLabels` | boolean | PR summary | Its sections, on when absent. |
| `showChecks`, `showReviews` | boolean | PR list | Its two filled columns, on when absent. |
| `showDetails`, `showLabels`, `showComments` | boolean | issue summary | Its sections, on when absent. |

Keys of each type, in form order:

- `github.pr.summary`: `repository`, `number`, `showChecks`, `showReviews`, `showPeople`, `showLabels`. `github.pr.page`, `github.issue.page`: `repository`, `number`. `github.issue.summary`: `repository`, `number`, `showDetails`, `showLabels`, `showComments`.
- `github.pr.list`: `scope`, `repository`, `owner`, `preset`, `filter`, `limit`, `pageSize`, `showChecks`, `showReviews`. `github.issue.list`: the same without the last two.
- `github.run`: `repository`, `branchSource`, `branch`, `number`, `workflow`.
- `github.run.list`: `repository`, `source`, `branch`, `number`, `workflow`, `limit`, `pageSize`.

Choice labels: scopes "One repository", "One owner", "All"; presets as GitLab's ("Created by me", "Review requested from me", "Assigned to me", "All open"); branch sources "Fixed branch", "Current branch of the main folder", "Branch of a linked pull request"; sources "All runs", "Started by me", "Of a branch", "Of a pull request".

**Settings screen** `general`, "General Configuration", in `GitHubSettings.cpp`:

| Key | Type | Description |
|---|---|---|
| `baseUrl` | URL | The GitHub web address, such as `https://github.example.com`. `https://github.com` by default, and when empty. |
| `token` | secret | A personal access token. Kept in the keychain under `<workspace id>/<profile id>/github/token`. |
| `repositories` | text list | Repositories the user works on, `owner/repo`, one per line. Cleaned as GitLab's `projects`. |

**Project setting** `repository`, "GitHub repository", read as `github.repository`.

**Server.** `GitHubServerType`, in `GitHubSettings.h`:

- the screen is `general` and the token field is `token`;
- `webBaseOf(values)` is the screen's `baseUrl`, or `https://github.com`; `apiBaseOf(values)` is `https://api.github.com/` when the web base's host is `github.com` or `www.github.com`, else the web base with `api/v3/` appended, without doubling a slash. `baseUrl()` returns `apiBaseOf()`, so `ApiClient` paths are `repos/...`, `search/issues`, `user`;
- `authorize()` sets `Authorization: Bearer <token>`, `Accept: application/vnd.github+json`, `X-GitHub-Api-Version: 2022-11-28` and `User-Agent: Mirante`. GitHub refuses requests without a user agent;
- `problem()` is always empty.

**Token.** A fine-grained token needs read access to Metadata, Pull requests, Issues, Checks, Commit statuses and Actions, and write access to Actions for re-run and cancel. A classic token needs `repo` (and `public_repo` alone for public repositories). A `403` on an action says "The token cannot do this. It needs write access to Actions."

**Pages,** in `GitHubUrls.cpp`, from the web base, without doubling a slash:

| Type | Address |
|---|---|
| `github.pr.page` | `<web>/<repository>/pull/<number>` |
| `github.issue.page` | `<web>/<repository>/issues/<number>` |

Messages: a base URL that is not `http` or `https`: "Set the GitHub base URL in this profile's Modules > GitHub > General Configuration, in the Profiles menu."; without a repository or number: "Set the repository and number in the item's settings."

**Items from a link.** `matchUrl()` reads `<web>/<owner>/<repo>/<kind>[/<rest>]` when the address is below the web base of the profile the new item would use, or on `github.com` whatever the profile. A query and a fragment are ignored, except where the table reads them. A repository's name keeps its case.

| Page | Types | Configuration | Name |
|---|---|---|---|
| `pull/<n>[/<tab>]` | `github.pr.summary`, `github.pr.page` | `repository`, `number` | `owner/repo#42` |
| `pull/<n>[/<tab>]` | `github.run` | `repository`, `branchSource` = `pullRequest`, `number` | `owner/repo#42 CI` |
| `issues/<n>` | `github.issue.summary`, `github.issue.page` | `repository`, `number` | `owner/repo#7` |
| `pulls` | `github.pr.list` | `scope` = `repository`, `repository`, `preset` = `allOpen` | `owner/repo PRs` |
| `issues` | `github.issue.list` | `scope` = `repository`, `repository`, `preset` = `allOpen` | `owner/repo issues` |
| `actions` | `github.run`, then `github.run.list` | run: `repository`, `branchSource` = `fixed`, empty `branch`; list: `repository`, `source` = `all` | `owner/repo CI`, `owner/repo runs` |
| `actions/workflows/<file>` | `github.run`, then `github.run.list` | as `actions`, with `workflow` = `<file>` | `owner/repo <file> CI`, `owner/repo <file> runs` |
| `actions/runs/<id>[/...]` | `github.run.list` | `repository`, `source` = `all` | `owner/repo runs` |
| `tree/<branch>` | `github.run` | `repository`, `branchSource` = `fixed`, `branch` | `owner/repo <branch> CI` |

On the web base itself, without an owner: `pulls` gives `github.pr.list` with `scope` = `all` and `preset` = `createdByMe`, named "My PRs"; `pulls/assigned` gives `assignedToMe`, "PRs assigned to me"; `pulls/review-requested` gives `reviewRequested`, "PRs to review"; `issues` gives `github.issue.list` with `createdByMe`, "My issues"; `issues/assigned` gives `assignedToMe`, "Issues assigned to me". Other pages, such as a repository's home page, `compare/...` or `pull/new/...`, are refused. A branch with a slash in `tree/<branch>` is taken whole.

**Tests:** `tst_register.cpp` (identity, ids, names, fields, server type, settings screen, project setting), `tst_settings.cpp` (web and API bases, including Enterprise and a base with a path; headers of `authorize()`; repository suggestions), `tst_urls.cpp` (page addresses and messages), `tst_from_url.cpp` (every row above, Enterprise, refused pages).

## Part 3: the client and status

**`GitHubClient`**, a thin copyable wrapper over an `ApiClient` and a receiver, as `GitLabClient`. A repository's owner and name are percent-encoded each on its own, not as one value: `repos/<owner>/<repo>/...`. A branch is percent-encoded in its query. Lists set `followLinks` and ask for `per_page` of at most 100 and of at most the limit.

| Method | Request | `arrayKey` |
|---|---|---|
| `currentUser` | `GET user` | |
| `repository` | `GET repos/<r>` | |
| `pullRequest` | `GET repos/<r>/pulls/<n>` | |
| `pullRequestReviews` | `GET repos/<r>/pulls/<n>/reviews`, up to 100 | |
| `repositoryPullRequests` | `GET repos/<r>/pulls` with a query, such as `state=open&sort=updated&direction=desc` | |
| `checkRuns` | `GET repos/<r>/commits/<sha>/check-runs`, up to 100 | `check_runs` |
| `combinedStatus` | `GET repos/<r>/commits/<sha>/status` | |
| `issue` | `GET repos/<r>/issues/<n>` | |
| `issueComments` | `GET repos/<r>/issues/<n>/comments?per_page=<k>&page=<p>`, one page | |
| `searchIssues` | `GET search/issues?q=<query>&sort=updated&order=desc`, up to the limit | `items` |
| `workflowRuns` | `GET repos/<r>/actions/runs`, or `repos/<r>/actions/workflows/<file>/runs` with a workflow, with a query such as `branch=<b>`, `actor=<login>` or `event=pull_request` | `workflow_runs` |
| `runJobs` | `GET repos/<r>/actions/runs/<id>/jobs?filter=latest`, up to 100 | `jobs` |
| `rerunRun`, `rerunFailedJobs`, `cancelRun` | `POST repos/<r>/actions/runs/<id>/rerun`, `/rerun-failed-jobs`, `/cancel` | |
| `rerunJob` | `POST repos/<r>/actions/jobs/<id>/rerun` | |

The `POST` methods send an empty JSON object. Tested in `tests/tst_client.cpp`.

**Status,** in `GitHubStatus.cpp`, with pure functions `pullRequestStatus()`, `issueStatus()`, `runStatus()` and `checksState()`, declared in `GitHubStatus.h` so views use the same rules. "Me" is `GET user`, asked in each job that needs it.

**`checksState(checkRuns, combinedStatus)`** reads a pull request's head commit:

- `failed` when a check run's `conclusion` is `failure`, `timed_out` or `startup_failure`, or the combined status has statuses and its `state` is `failure` or `error`;
- else `running` when a check run's `status` is not `completed`, or the combined status has statuses and its `state` is `pending`;
- else `passed` when there is at least one check run or status;
- else `none`.

A `cancelled` or `action_required` conclusion is neither failed nor passed alone; it counts as `passed` for the summary if everything else passed, and shows its own pill in the view.

| Types | Requests | Attention | Closed |
|---|---|---|---|
| `github.pr.summary`, `github.pr.page` | the pull request, its reviews and the current user, at once; then, at once, `checkRuns` and `combinedStatus` of `head.sha` | "Checks failed" (`checks-failed`, notify) when `checksState` is `failed`; "Review requested" (`review`) when I am in `requested_reviewers`; "Approved" (`approved`, clears on view) when I am the author and the reviews approve; "Changes requested" (`changes-requested`) when I am the author and a latest review is `CHANGES_REQUESTED`; "Has conflicts" (`conflicts`) when I am the author and `mergeable` is `false` | `state` is `closed` |
| `github.issue.summary`, `github.issue.page` | the issue; for the summary, then its latest comments | none | `state` is `closed` |
| `github.run` | for `pullRequest`, the pull request, for `head.ref` and `head.sha`; for an empty `branch`, the repository, for `default_branch`; for `mainFolder`, the folder's branch; then `workflowRuns` of the branch, up to 20; then the jobs of each run in the [group](#the-run-group), at once | "Run failed" (`run-failed:<id>:<run_attempt>`, notify) for each run of the group whose `conclusion` is `failure`, `timed_out` or `startup_failure` | never |

- **Only open pull requests need attention.** A closed or merged one reports no reasons.
- **Reviews.** Each reviewer's latest review counts, by `submitted_at`; `COMMENTED` and `PENDING` reviews do not replace an earlier `APPROVED` or `CHANGES_REQUESTED`, and `DISMISSED` removes it. The reviews approve when at least one latest review is `APPROVED` and none is `CHANGES_REQUESTED`.
- **Summaries.** A pull request reads like "Open · 2 approvals · checks failed": "Open", "Draft", "Merged" (when `merged` or `merged_at` is set) or "Closed"; the number of approvals, or "changes requested"; the checks as "checks passed", "checks failed" or "checks running", left out for `none`; "has conflicts" while open. An issue reads like "Open · assigned to ana". A run group reads like "Failed · main · 1 of 3 runs failed", "Running · main · 3 runs" or "Passed · main · 3 runs"; a single run leaves out the count and names its workflow: "Passed · main · CI".
- **A branch without runs** is `Ok` with "No runs on <branch>".
- **The main folder's branch,** an incomplete configuration and errors behave as in GitLab: "Set the main folder's path on this device.", "The main folder is not on a branch.", "Set the repository and number in the item's settings.", with `State::None` and no request, and `statusFromReply()` for a failed reply. The pull request's own reply wins over the reviews and the user. A failed `checkRuns` or `combinedStatus` leaves that part out, and `checksState` reads what came. A failed request for comments or a sibling run's jobs is not an error. A failed request for the lead run's jobs is.
- **Comments.** The comments endpoint gives the oldest first and has no sort. The summary reads the issue's `comments` count, asks for the last page with `per_page=20`, and, if that page holds fewer than 5 and is not the first, the page before it. `comments` in `data` is newest first, at most 20.
- **Data.** For a pull request: `pullRequest`, `reviews` (GitHub's answer), `user` (`id` and `login`), `checkRuns` (the array), `combinedStatus` (the object). For an issue: `issue`, and `comments` for the summary. For a run: `branch`, `headSha`, `run` (the lead), `jobs` (the lead's), `siblings` (`[{"run": ..., "jobs": [...]}]`, without `jobs` if that request failed), `older` (runs without jobs), and `pullRequest` when known. Page types store the same as the summary, without comments.

### The run group

`runGroup()` takes the runs of the answer, newest `created_at` first. The group's commit is the pull request's `head.sha` when `branchSource` is `pullRequest` and some run has that `head_sha`; otherwise the newest run's `head_sha`. The group is every run with that `head_sha`. The newest of them leads: its status, `run` and `jobs`. The others of the group are `siblings`; the rest are `older`.

The tests are in `tests/tst_status.cpp`, on answers in `tests/data/`. Record them with `gh api` from public repositories, such as `cli/cli`, trimmed by hand, with logins, names and emails replaced. Write by hand those that public repositories cannot give, such as a review with changes requested, and say so in the README.

## Part 4: the lists

`github.pr.list`, `github.issue.list` and `github.run.list` report status from `GitHubLists.cpp`, as GitLab's lists do: `Ok`, a count summary ("12 pull requests", "12 issues", "12 runs", singular forms, "100+ …" at the limit), `rows` and `total` in `data`, no attention, `closed` unset.

**`searchQuery()`** builds the query of the PR and issue lists from these parts, joined by spaces, in this order:

1. `is:pr` or `is:issue`, and `is:open`;
2. the scope: `repo:<repository>`, `user:<owner>`, or nothing for `all`;
3. the preset: `author:@me`, `review-requested:@me`, `assignee:@me`, or nothing for `allOpen`, except `involves:@me` for `allOpen` with scope `all`;
4. the trimmed `filter`, as typed.

A preset the type does not have counts as its first one. An incomplete scope reports `State::None` with "Set the repository in the item's settings." or "Set the owner in the item's settings.", without a request. An empty `repository` uses the project's setting.

**`listWebUrl()`**: `<web>/<repository>/pulls?q=<query>` for the repository scope, `<web>/pulls?q=<query>` otherwise, and `/issues` for issues. The query is the same as the search's, percent-encoded. Empty when the scope is incomplete.

**`fetchRunListStatus()`** serves `github.run.list`, with `workflowRuns()`, filtered by `workflow` when set:

| Source | Request |
|---|---|
| `all` | the repository's runs |
| `mine` | `currentUser()`, then `actor=<login>`. If the first fails, the list is not asked for. |
| `branch` | `branch=<branch>`; an empty branch first reads the repository's `default_branch` |
| `pullRequest` | `pullRequest()`, then `branch=<head.ref>&event=pull_request` |

Its Web link is `<web>/<repository>/actions`, or `<web>/<repository>/actions/workflows/<file>` with a workflow.

Tests in `tests/tst_list_query.cpp`, on hand-written `pr_search.json`, `issue_search.json` and `run_list.json`.

## Part 5: the summary views

`GitHubPullRequestView.cpp`, `GitHubRunView.cpp` and `GitHubIssueView.cpp`, each a subclass of `GitHubSummary` in `GitHubViewSupport.h`, which does what `GitLabSummary` does: drawing from the status, actions with a busy button and a refresh, "Open page" and the Web link, dialogs on the heap.

**GitHub PR (summary).** Top: the title; pills for the state and for a draft; `head.ref` → `base.ref`. Sections:

- Checks (`showChecks`): a pill for `checksState`, and each check run and status that did not pass, with its name, a pill and a "Details" link to its `html_url` or `details_url` (or a status's `target_url`), in the external browser. When all passed, the count: "12 checks passed".
- Reviews (`showReviews`): each reviewer's latest review, with a pill (Approved, Changes requested, Commented), and the requested reviewers who have not reviewed yet, as "Waiting".
- People (`showPeople`): the author, the assignees.
- Labels (`showLabels`).

Actions: "Open page" and "Web". No re-run; see decision 0102.

**GitHub Actions Run.** As GitLab's pipeline view:

- **Top:** the run shown, at first the lead: its workflow name and `#<run_number>`; a pill for its status; the branch, the duration (`updated_at` − `run_started_at` once completed) and when it started. With more than one run in the group, a row of pills, one per run, named by workflow, switches between them. "Previous" and "Next" step through all runs of the status, the group and `older`, in time order.
- **Jobs:** one table of the shown run's jobs, in their order, each with a pill, the name, the duration and links: "Log" (the job's `html_url`, external browser) on a job that started; "Re-run" on a completed job. An older run reads its jobs with `runJobs()` when shown, as GitLab's view does.
- **Actions:** "Re-run failed jobs" when the shown run's conclusion is `failure`, `timed_out` or `startup_failure`; "Re-run" when it is completed; "Cancel run" while `queued`, `in_progress`, `waiting`, `requested` or `pending`, after a confirmation; "Pull request", opening the linked pull request, or for a fixed or main-folder branch the run's `pull_requests[0]`, in the external browser, hidden without one; "Open page" and "Web" on the shown run's `html_url`.
- **Refresh** every 15 seconds while the shown run is not `completed` and the view is visible.
- Action ids: `rerun-failed`, `rerun-run`, `cancel-run`, `pull-request`, `open-page`; a job's `log-<id>` and `rerun-job-<id>`; pills `run-pill-<id>`; `previous-run`, `next-run`; the timer `run-refresh`.

**Status pills.** `status` other than `completed`: `queued`, `waiting`, `requested`, `pending` are Waiting "Queued" / "Waiting"; `in_progress` is Waiting "Running". `completed` takes its `conclusion`: `success` Good "Passed"; `failure`, `timed_out`, `startup_failure` Bad "Failed", "Timed out", "Failed"; `cancelled` Neutral "Cancelled"; `skipped` Neutral "Skipped"; `neutral` Neutral "Neutral"; `action_required` Waiting "Action required"; `stale` Neutral "Stale". The same for check runs.

**GitHub Issue (summary).** As GitLab's: title and state pill (`state_reason` `completed` or `not_planned` reads "Closed" either way); Details (`showDetails`: assignees, milestone); Labels; Comments (`showComments`: the latest five, newest first, plain text, "No comments", "The comments could not be read."). Actions: "Open page" and "Web".

Tests in `tests/tst_summary_views.cpp`, with a fake context and a fake status source, on the status tests' data.

## Part 6: the list views

`GitHubPullRequestListView.cpp`, `GitHubIssueListView.cpp` and `GitHubRunListView.cpp`, subclasses of `GitHubList` in `GitHubListSupport.h`, as GitLab's lists. A search row's repository comes from `repository_url` (`.../repos/<owner>/<repo>`); its key is `owner/repo#42`.

- **PR List** columns: Ref (`repo#42`, the repository's name only), Title, Author, Checks (a pill), Reviews (such as "2 approved", "changes requested" or a dash), Updated, links. A narrow cell hides Author, Reviews, Updated, Checks, Ref, in that order. Checks and Reviews are filled by the view for the first 25 visible rows, as GitLab's Pipeline and Approvals: Checks asks `pullRequest()` and then `checkRuns()` and `combinedStatus()` of its `head.sha`; Reviews asks `pullRequestReviews()`. The rules of GitLab's view for pages, sorts, receivers and kept answers apply unchanged.
- **Issue List** columns: Ref, Title, Assignees, Labels, Updated, links. No further requests.
- **Run List** columns: Run (`#<run_number>`), Workflow, Status (a pill), Branch (the free width), Actor (`actor.login`), Started (`run_started_at`, else `created_at`), links. A narrow cell hides Actor, Started, Workflow, Run, Status. Filters: Status and Workflow. Sorted on Run, newest first.
- **Links:** "Web", "Open" and "Add", and the row menu, as GitLab's. "Add" adds a GitHub PR (summary) named `owner/repo#42`, a GitHub Issue (summary) named `owner/repo#7`, or a GitHub Actions Run: for a row whose `event` is `pull_request` with an entry in `pull_requests`, `branchSource` `pullRequest` and that number, named `owner/repo#42 CI`; otherwise `fixed` with `head_branch`, named `owner/repo <branch> CI`. The item's `workflow` is kept in the added run, if set.
- **Empty lines:** "No pull requests", "No issues", "No runs".

Tests in `tests/tst_list_views.cpp`.

## Part 7: quick add

`GitHubQuickAdd.cpp`, as GitLab's:

- **`GitHubQuickAddHandler`** takes a pull request's address below the web base of the dialog's profile, or on `github.com`. It asks for the pull request, and proposes the title; `github.pr.summary` named `#42` and `github.run` named `#42 CI` with `branchSource` `pullRequest`; the project whose setting `repository` is the address's repository, compared without case; and the texts title and `head.ref`.
- **`GitHubRelatedItemFinder`** reads Jira-style keys as GitLab's. With the project setting `repository`: one `repositoryPullRequests()` with `state=open&sort=updated&direction=desc`, up to 100, matching `title` or `head.ref`, without case. Without it: one `searchIssues()` with `is:pr is:open in:title (<key1> OR <key2> …)` and a `repo:` qualifier for each repository of the profile's General Configuration, up to 100, with the title checked for the key again. Without listed repositories it finds nothing, because a search without `repo:` would cover all of GitHub. Each match is a `github.pr.summary` named `#42` in the setting's repository, else `owner/repo#42`.

Tests in `tests/tst_quick_add.cpp`.

## Part 8: documents

- `src/modules/github/README.md`, in the shape of the GitLab README.
- [Modules](../modules.md): a row in the table of modules.
- [Item types](../item-types.md): a `## GitHub` section after `## GitLab`, with the layouts above.
- [Integrations](../integrations.md): a `## GitHub` section after `## GitLab`: API, server, authentication, token, caching, paging, rate limits, endpoints, and what each type asks for.
- [Overview](../overview.md) and the [glossary](../README.md#glossary), where they name GitLab and Jira as the services, name GitHub too.
- [Decision 0102](../decisions/0102-github-module.md) becomes Accepted when the user accepts M11.
- `translations/mirante_pt_BR.ts`: the `github` context, at the end. "Pull request" stays "pull request" in Portuguese, as GitHub's own Portuguese interface does.

## Tasks for parallel work

As [decision 0017](../decisions/0017-parallel-tasks-with-fixed-interfaces.md) says: T0 writes every header with stubs that compile; the others implement behind them; agents commit on their own branch and do not merge.

| Task | Branch | Work | Owns | Needs |
|---|---|---|---|---|
| T0 | `m11-interfaces` | `GetOptions::arrayKey` in `ApiClient.h`. The module folder: `CMakeLists.txt` with every source and test named below, the icon, `GitHubModule.cpp` registering identity, types, fields, settings screen, project setting and server type, wired to stub functions. Every header of the module with its declarations: `GitHubClient.h`, `GitHubSettings.h`, `GitHubUrls.h`, `GitHubStatus.h`, `GitHubLists.h`, `GitHubQuickAdd.h`, `GitHubViewSupport.h`, `GitHubListSupport.h`, the six view headers. `GitHubClient.cpp` implemented in full, with `tst_client.cpp`, because every other task calls it. Stubs for every other `.cpp`, and an empty test case in each test file. The README with Part 2's tables. The documents of Part 8, except translations. | everything it creates; after merging, the headers belong to the orchestrator | none |
| T1 | `m11-core` | Part 1. | `src/api/ServerClient.cpp` and its tests | T0 |
| T2 | `m11-github-basics` | Part 2: `matchUrl()` and `fromUrl()`, and tests of the rest. T0 implemented the settings, the server type, the page addresses and `readPageAddress()`, because T3, T4 and T7 call them. | `GitHubSettings.cpp`, `GitHubUrls.cpp`, `tst_register.cpp`, `tst_settings.cpp`, `tst_urls.cpp`, `tst_from_url.cpp`, `GitHubModule.cpp` | T0 |
| T3 | `m11-github-status` | Part 3. | `GitHubStatus.cpp`, `tst_status.cpp`, `tests/data/` except the list files | T0 |
| T4 | `m11-github-lists` | Part 4. | `GitHubLists.cpp`, `tst_list_query.cpp`, `tests/data/pr_search.json`, `issue_search.json`, `run_list.json` | T0 |
| T5 | `m11-github-summaries` | Part 5. | `GitHubViewSupport.cpp`, the three summary views, `tst_summary_views.cpp` | T3 |
| T6 | `m11-github-list-views` | Part 6. | `GitHubListSupport.cpp`, the three list views, `tst_list_views.cpp` | T4, T5 |
| T7 | `m11-github-quick-add` | Part 7. | `GitHubQuickAdd.cpp`, `tst_quick_add.cpp`, `tests/data/quick_add_*.json` | T0 |
| T8 | none | The orchestrator: merge in order, translate the `github` context, write `docs/plans/m11-checks.md`, run the `dev`, `no-modules`, `release` and `ci` presets, and tick the roadmap once the user has checked. | any | all |

### Waves

1. T0 alone. The user may review its headers before the others start.
2. T1, T2, T3, T4 and T7 at once: five agents.
3. T5, once T3 is merged.
4. T6, once T4 and T5 are merged.
5. T8.

Merge order within a wave: T1, T2, T3, T4, T7, so the core and the basics land first. Each merge is built and tested with `master` merged in.

## Checking

- **Automatic:** the tests above, by CTest locally and in CI on Linux, with all modules and with none. No test uses the network or the real keychain.
- **By hand on Windows,** listed in `docs/plans/m11-checks.md` by T8, against github.com with the user's token:
  - each summary and list shows real data, in a wide and a narrow cell; each section's field hides it;
  - "Review requested from me" and "Assigned to me" across repositories; an owner scope; a custom filter;
  - re-run a failed job, re-run failed jobs, cancel a run, on a scratch repository; a token without Actions write shows the clear message, and the server keeps working;
  - pasting each link of Part 2's table; quick add from a pull request address, with related items found by a Jira key;
  - a rate limit, by hand only if one is met; otherwise noted as checked by the tests alone;
  - GitHub Enterprise Server, if one is at hand; otherwise noted as not checked.

## Out of scope

- Starting a run by hand (`workflow_dispatch`).
- Re-running a pull request's check runs, and merging, approving or commenting from the summary. See [decision 0070](../decisions/0070-read-mostly-native-views.md).
- GitHub Discussions, Projects, releases and notifications.
- Sharing view code between the `gitlab` and `github` modules through the view kit.

## Ideas for later

- **Run a workflow** with `workflow_dispatch`, for a run item that names a workflow, with a dialog for its inputs.
- **Notifications** (`GET notifications`) as a list, which would show what GitHub itself thinks needs attention.
- **Move shared summary and list code to the view kit,** once a third service needs it.
