# M11 checks by hand

These are the checks for [milestone M11](../roadmap.md#m11-github-module) that tests cannot make. Run them on Windows with the release build:

```powershell
task build PRESET=release
task run PRESET=release -- -Data <a test workspace> -Settings <a scratch .ini file>
```

You need a GitHub personal access token in a profile's Modules > GitHub > General Configuration, a repository you can push to for the actions (a scratch repository with a workflow that can fail on demand is best), and a task to add items to.

Tick each box, or note what is wrong next to it.

## Settings and links

- [ ] Profiles > <profile> > Modules > GitHub opens General Configuration with Base URL, Token and Repositories. An empty base URL means github.com.
- [ ] Help > Modules lists "GitHub" with its eight types, its version, author and homepage.
- [ ] A project's dialog has "GitHub repository". Items with an empty repository use it.
- [ ] Every item's Repository field offers the repositories of the profile's General Configuration.
- [ ] Pasting each of these into Add Item… offers the right types, with the configuration and name filled:
  - [ ] a pull request, also with `/files` after its number;
  - [ ] an issue;
  - [ ] a repository's `pulls` and `issues` pages;
  - [ ] a repository's `actions` page, and one workflow's page under `actions/workflows/`;
  - [ ] a run's page under `actions/runs/`;
  - [ ] a branch's `tree/<branch>` page, also with a slash in the branch;
  - [ ] `github.com/pulls`, `github.com/pulls/review-requested` and `github.com/issues/assigned`.
- [ ] A repository's home page and `compare/...` are refused.

## Pull requests

- [ ] GitHub PR (summary) shows the title, state, branches, checks, reviews, people and labels, in a wide and a narrow cell.
- [ ] Each section's field in the item's settings hides that section.
- [ ] A failed check shows with a "Details" link that opens in the browser. All passed shows the count.
- [ ] Reviews show each reviewer's latest review, and requested reviewers who have not reviewed as "Waiting".
- [ ] The cell header shows "Checks failed" with a desktop notification on a watched item; "Review requested" on a pull request that asks for your review; "Approved" on yours once approved, which clears after you look.
- [ ] A merged or closed pull request shows no attention, and the task can be marked done.
- [ ] "Open page" shows the page in the cell, with a way back; `Shift` opens the browser. "Web" opens the browser.
- [ ] GitHub PR (page) shows the page logged in after one login in the profile, with the same badge as the summary.

## Actions runs

- [ ] GitHub Actions Run on a fixed branch shows the newest commit's runs, one pill per workflow, and the shown run's jobs.
- [ ] An empty branch follows the repository's default branch.
- [ ] "Current branch of the main folder" follows a branch switch in the folder.
- [ ] "Branch of a linked pull request" shows that pull request's head commit, and "Pull request" opens it.
- [ ] Previous and Next step through older runs, whose jobs load when shown.
- [ ] A running run refreshes about every 15 seconds until it finishes.
- [ ] On the scratch repository: "Re-run failed jobs", "Re-run", a job's "Re-run" and "Cancel run" (after its question) work, and the view refreshes.
- [ ] A token without Actions write access shows "The token cannot do this. It needs write access to Actions." and the server keeps working.
- [ ] A failed run notifies on a watched item. Re-running it and failing again notifies again.
- [ ] A branch without runs shows "No runs on <branch>".

## Issues

- [ ] GitHub Issue (summary) shows the title, state, assignees, milestone, labels and the latest five comments, newest first.
- [ ] An issue without comments shows "No comments".
- [ ] GitHub Issue (page) shows the page.

## Lists

- [ ] GitHub PR List with "Review requested from me", "Assigned to me" and "Created by me", scope All, shows pull requests across repositories.
- [ ] Scope "One owner" with an organisation shows its pull requests.
- [ ] A custom filter such as `label:bug` narrows the list.
- [ ] Checks and Reviews fill in for the visible rows. Turning either field off hides its column.
- [ ] The Web link above the table opens the same search on GitHub.
- [ ] GitHub Issue List shows issues the same way.
- [ ] GitHub Actions Run List shows runs with their workflow, status, branch and actor. The Status and Workflow filters work.
- [ ] "Started by me" and "Of a pull request" show the right runs.
- [ ] "Add" on a row adds the right item to the task: a PR summary, an issue summary, or a run on the row's branch or pull request.
- [ ] A narrow cell hides columns in the documented order.

## Quick add

- [ ] Quick add with a pull request's address proposes its title, a PR summary and a run item, and picks the project whose GitHub repository matches.
- [ ] A Jira key in the title finds other open pull requests with the key, in the project's repository.
- [ ] A github.com address does nothing in a profile set to a GitHub Enterprise Server.

## Limits and other servers

- [ ] Many lists at once do not pause the GitHub server. If the search rate limit is met, the lists wait and recover. If it is not met, note "checked by the tests only".
- [ ] GitHub Enterprise Server, if one is at hand: items read from `<host>/api/v3`, and links to `<host>` are recognised. Otherwise note "not checked".
- [ ] Portuguese: with the app in Portuguese, the GitHub texts read naturally.
