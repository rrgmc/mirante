# 0068. Views ask the server directly only for what the user asked for

- Status: Proposed
- Date: 2026-10-30

## Context

[Integrations](../integrations.md#polling-and-caching) says that modules fetch through the status cache, never around it. The cache keeps one status per item. Some data in M6 does not belong to an item's status: the preview of a list row, the transitions a Jira issue allows now, and the pipeline and approvals columns of a merge request list.

## Decision

- **An item's own data comes from the status cache.** That includes everything a summary shows. A summary type may ask for more than its page type, such as a Jira description or an issue's comments, so a page item stays cheap.
- **A view may use `ItemContext::server()` directly** for data that a user's action or a visible list needs, and that is not the item's status: a row's preview, Jira transitions, and the two merge request list columns. It also uses it for the light actions.
- **After an action succeeds,** the view calls `refresh()` on its status, so the cell, the badges and the view all change together.
- **These requests use the same `ApiClient`,** so they keep the token handling, ETags, the limit of four requests at once and the error rules.

## Alternatives

- **A cache entry for each row and each transition list.** The cache would need entries without an item, and rules for when they expire, for data that is only needed while it is on screen.
- **A status job that fetches every row's detail.** It would cost many requests for rows nobody looks at.

## Consequences

- The sentence in integrations changes: modules poll through the cache.
- A preview does not refresh on the poll interval. It reads again when the list's status changes, and after an action.
