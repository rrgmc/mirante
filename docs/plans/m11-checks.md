# M11 checks by hand

These are the checks for [milestone M11](../roadmap.md#m11-github-module) that tests cannot make. Run them on Windows with the release build:

```powershell
task build PRESET=release
task run PRESET=release -- -Data <a test workspace> -Settings <a scratch .ini file>
```

You need a GitHub personal access token in a profile's Modules > GitHub > General Configuration, a repository you can push to for the actions (a scratch repository with a workflow that can fail on demand is best), and a task to add items to.

Tick each box, or note what is wrong next to it.

**Result, 2026-10-30:** the user ran the checks with the release build and accepted M11. Every box is ticked.

## Settings and links

- [x] Profiles > <profile> > Modules > GitHub opens General Configuration with Base URL, Token and Repositories. An empty base URL means github.com.
- [x] Help > Modules lists "GitHub" with its eight types, its version, author and homepage.
- [x] A project's dialog has "GitHub repository". Items with an empty repository use it.
- [x] Every item's Repository field offers the repositories of the profile's General Configuration.
- [x] Pasting each of these into Add Item… offers the right types, with the configuration and name filled:
  - [x] a pull request, also with `/files` after its number;
  - [x] an issue;
  - [x] a repository's `pulls` and `issues` pages;
  - [x] a repository's `actions` page, and one workflow's page under `actions/workflows/`;
  - [x] a run's page under `actions/runs/`;
  - [x] a branch's `tree/<branch>` page, also with a slash in the branch;
  - [x] `github.com/pulls`, `github.com/pulls/review-requested` and `github.com/issues/assigned`.
- [x] A repository's home page and `compare/...` are refused.

## Pull requests

- [x] GitHub PR (summary) shows the title, state, branches, checks, reviews, people and labels, in a wide and a narrow cell.
- [x] Each section's field in the item's settings hides that section.
- [x] A failed check shows with a "Details" link that opens in the browser. All passed shows the count.
- [x] Reviews show each reviewer's latest review, and requested reviewers who have not reviewed as "Waiting".
- [x] The cell header shows "Checks failed" with a desktop notification on a watched item; "Review requested" on a pull request that asks for your review; "Approved" on yours once approved, which clears after you look.
- [x] A merged or closed pull request shows no attention, and the task can be marked done.
- [x] "Open page" shows the page in the cell, with a way back; `Shift` opens the browser. "Web" opens the browser.
- [x] GitHub PR (page) shows the page logged in after one login in the profile, with the same badge as the summary.

## Actions runs

- [x] GitHub Actions Run on a fixed branch shows the newest commit's runs, one pill per workflow, and the shown run's jobs.
- [x] An empty branch follows the repository's default branch.
- [x] "Current branch of the main folder" follows a branch switch in the folder.
- [x] "Branch of a linked pull request" shows that pull request's head commit, and "Pull request" opens it.
- [x] Previous and Next step through older runs, whose jobs load when shown.
- [x] A running run refreshes about every 15 seconds until it finishes.
- [x] On the scratch repository: "Re-run failed jobs", "Re-run", a job's "Re-run" and "Cancel run" (after its question) work, and the view refreshes.
- [x] A token without Actions write access shows "The token cannot do this. It needs write access to Actions." and the server keeps working.
- [x] A failed run notifies on a watched item. Re-running it and failing again notifies again.
- [x] A branch without runs shows "No runs on <branch>".

## Issues

- [x] GitHub Issue (summary) shows the title, state, assignees, milestone, labels and the latest five comments, newest first.
- [x] An issue without comments shows "No comments".
- [x] GitHub Issue (page) shows the page.

## Lists

- [x] GitHub PR List with "Review requested from me", "Assigned to me" and "Created by me", scope All, shows pull requests across repositories.
- [x] Scope "One owner" with an organisation shows its pull requests.
- [x] A custom filter such as `label:bug` narrows the list.
- [x] Checks and Reviews fill in for the visible rows. Turning either field off hides its column.
- [x] The Web link above the table opens the same search on GitHub.
- [x] GitHub Issue List shows issues the same way.
- [x] GitHub Actions Run List shows runs with their workflow, status, branch and actor. The Status and Workflow filters work.
- [x] "Started by me" and "Of a pull request" show the right runs.
- [x] "Add" on a row adds the right item to the task: a PR summary, an issue summary, or a run on the row's branch or pull request.
- [x] A narrow cell hides columns in the documented order.

## Quick add

- [x] Quick add with a pull request's address proposes its title, a PR summary and a run item, and picks the project whose GitHub repository matches.
- [x] A Jira key in the title finds other open pull requests with the key, in the project's repository.
- [x] A github.com address does nothing in a profile set to a GitHub Enterprise Server.

## Limits and other servers

- [x] Many lists at once do not pause the GitHub server. If the search rate limit is met, the lists wait and recover. If it is not met, note "checked by the tests only".
- [x] GitHub Enterprise Server, if one is at hand: items read from `<host>/api/v3`, and links to `<host>` are recognised. Otherwise note "not checked".
- [x] Portuguese: with the app in Portuguese, the GitHub texts read naturally.
