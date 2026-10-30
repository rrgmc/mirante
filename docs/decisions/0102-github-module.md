# 0102. A GitHub module, shaped like the GitLab module

- Status: Proposed
- Date: 2026-10-30

## Context

The user works on GitHub as well as GitLab, and wants the same panels for both: pull requests, CI runs and issues, as summaries, pages and lists, with status, attention, pasted links and quick add. The GitLab module ([its README](../../src/modules/gitlab/README.md)) already has all of these for GitLab.

GitHub differs from GitLab in ways that change the design:

- **Two addresses.** The web pages are at `https://github.com`, the API at `https://api.github.com`. GitHub Enterprise Server has both on one host: pages at `<host>`, the API at `<host>/api/v3`.
- **No group or dashboard lists.** The REST API lists pull requests and issues of one repository only. Lists across repositories come from the search API, `GET /search/issues`, which takes a query such as `is:pr is:open review-requested:@me`.
- **Answers that wrap their rows.** Search, workflow runs, jobs and check runs answer with an object, such as `{"total_count": 3, "workflow_runs": [...]}`, not with an array. `ApiClient`'s `followLinks` joins arrays only.
- **Rate limits answer `403`.** When the hourly limit is used up, GitHub answers `403` with `x-ratelimit-remaining: 0`, or `403` with `Retry-After` for its secondary limits. `ServerClient` treats a `403` on a `GET` as a refused token and pauses the server.
- **CI is many workflows per commit.** One push starts one run of each workflow that matches it. Runs of one commit share its `head_sha`. A re-run keeps the run's id and raises its `run_attempt`.
- **No required approvals count.** Branch protection, which holds it, needs admin rights to read.

## Decision

Add a module `github`, built like `gitlab`, with eight item types: `github.pr.summary`, `github.pr.page`, `github.pr.list`, `github.run`, `github.run.list`, `github.issue.summary`, `github.issue.page` and `github.issue.list`. The [M11 plan](../plans/m11-github-module.md) gives the details. The choices that differ from GitLab:

- **One base URL, the web address.** The General Configuration holds the web address, `https://github.com` by default. The server type derives the API address from it: `https://api.github.com/` for `github.com`, `<base>/api/v3/` for any other host. Pages, pasted links and Web links use the web address.
- **Lists use the search API.** The pull request and issue lists build one search query from the scope (`repo:<repository>`, `user:<owner>` or none), the preset (`author:@me`, `assignee:@me`, `review-requested:@me`) and `is:open`. The filter is extra search qualifiers, such as `label:bug base:main`, not URL parameters. The scope "all" with the preset "All open" means `involves:@me`, because all of GitHub is not a useful list.
- **`GetOptions::arrayKey`.** With `followLinks`, a client can name the key of the array inside an object answer. The client joins those arrays across pages, up to `maxRows`, and answers with the first page's object holding all the rows.
- **Rate limits wait.** `ServerClient` treats a `403` or `429` with `x-ratelimit-remaining: 0` as a `429`: it holds the queue until `x-ratelimit-reset`. A `403` with `Retry-After` waits that long. Neither pauses the server. This is core code, written for GitHub's headers, but it harms no other server: GitLab and Jira do not send these headers with a `403`.
- **A run group is one commit.** `github.run` shows the runs of the newest commit of a branch, one per workflow, as GitLab's pipeline shows the pipelines started together ([decision 0072](0072-pipelines-started-together.md)). The commit is the head of the linked pull request, when there is one. A failed run's attention key holds its id and attempt, so a re-run that fails again counts again.
- **Reviews instead of approvals.** A pull request shows each reviewer's latest review. It is approved when at least one is `APPROVED` and none is `CHANGES_REQUESTED`.
- **Checks are read, not run.** A pull request's checks are its head commit's check runs and commit statuses. The summary links to each failed check. It offers no re-run, because a check run can be re-run only by the GitHub App that made it. `github.run` re-runs workflow runs and jobs.

## Alternatives

- **The GraphQL API.** One query could read a pull request with its reviews, checks and review decision. But `ApiClient` speaks REST with ETags, which make repeated polls cheap; GraphQL has no ETags and counts every query against the limit. The REST answers also match how the GitLab module is built and tested.
- **Repository lists only, no search.** No search rate limit (30 requests a minute), but no "review requested from me" or "assigned to me" across repositories, which are the lists the user opens most.
- **A separate API base URL setting.** It would let a user point the API elsewhere, but every real setup derives it from the web address, and a second field is a second thing to get wrong.
- **Paging in the module,** as Jira does. Each list method would repeat the paging loop. One option in the core serves all four wrapped answers.
- **Group runs by time, as GitLab does.** Runs of one push start within seconds, but so can runs of two quick pushes. The commit is exact.

## Consequences

- The user gets the same panels for GitHub as for GitLab, and learns one set of item types.
- Search lists count against GitHub's search limit of 30 requests a minute. Many lists polling at once can reach it, and then the client waits. The poll interval of lists keeps this rare.
- GitHub Enterprise Server is supported by its address alone. It is not checked by hand unless a server is at hand.
- The core gains two small, general features: `arrayKey` and rate-limit headers on `403`.
- The module duplicates some of the GitLab module's view code, because modules cannot depend on each other. Moving shared parts to the view kit is left for later.
