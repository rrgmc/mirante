# 0070. Native views are read-mostly

- Status: Accepted
- Date: 2026-10-30

## Context

M6 gave the summaries light actions: approve, comment, change a Jira status, close and reopen an issue. After trying them, the user found these too easy to trigger by mistake for something that changes other people's work. The web page is one click away for such changes. Running CI is different: it is repeatable, and it changes no one's review or issue.

## Decision

- **Removed:** approve a merge request, comment, change a Jira status, close and reopen an issue. The buttons, their client methods and their tests go.
- **Kept, and only for CI:** retry a pipeline, retry a job, cancel a pipeline, with its confirmation.
- **Added, for CI:** play a manual job, and trigger a new pipeline on the branch.
- **Every native view links out.** "Web" opens the external browser, and "Open page" opens the page in the cell.

This supersedes the actions in [item types](../item-types.md#light-actions), and the plan's choice to add close and reopen. A token with only `read_api` now loses only the CI actions.

## Alternatives

- **Keep the actions behind a confirmation.** Safer, but each is a dialog for something the web page does better, with the full context in view.
- **A setting to turn them on.** One more option, and more code to keep, for actions the user does not want.

## Consequences

- `ApiClient::put()` has no caller now. It stays, since it is small, tested and generic.
- The `api` token scope is needed only for CI actions.
