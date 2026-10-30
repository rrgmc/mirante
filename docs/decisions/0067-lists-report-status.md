# 0067. Lists report a status, without attention

- Status: Proposed
- Date: 2026-10-30

## Context

[Decision 0061](0061-attention-from-status-of-any-rendering.md) left to M6 what a list reports. A list needs polling, a refresh button and the same error display as other items, and the status cache already provides those. A list could also raise attention, for example when a new merge request waits for review, but it would then have to define which rows are new.

GitLab's merge request lists do not return the pipeline or the approvals of each row, so those two columns need further requests.

## Decision

- **Lists report status.** `hasStatus()` is true for the GitLab MR List, GitLab Issue List and Jira List. `data` holds the rows, and `summary` holds the count, such as "12 merge requests".
- **No attention.** A list reports no attention reasons, and leaves `closed` unset, so it never counts towards a task being done.
- **The pipeline and approvals columns** are filled by the view, not the status. While the view is visible, after each new status, it asks for the merge request and its approvals for the first 25 rows. Other rows show a dash until they are selected. The columns can be hidden, which also stops the requests. See [decision 0066](0066-fixed-native-layouts-with-sections.md).

## Alternatives

- **The view fetches its own rows.** It would lose the polling, the refresh button and the error display, or have to repeat them.
- **Attention for new rows.** Useful, but it needs a record of the rows seen, and a badge on a busy list would rarely clear. It can be added later.
- **GitLab's GraphQL API for the two columns.** One request could answer for all rows, but it adds a second API with its own errors and paging, for two columns.
- **Fetching the two columns for every row.** A list has up to 100 rows, so a poll could cost 200 requests.
- **Dropping the two columns from the table.** The preview would still show them, but the table is where a failed pipeline is most useful to spot.

## Consequences

- A list in a hidden Urgent or High task costs one or two requests per poll. The further requests run only while the view is visible.
- Sorting by pipeline or approvals is only meaningful among the first 25 rows.
