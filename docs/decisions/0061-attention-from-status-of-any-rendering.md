# 0061. Status comes from the item's object, and some attention clears when seen

- Status: Proposed
- Date: 2026-10-29

## Context

M5 adds the status cache and attention badges. Native summary views arrive only in M6, so in M5 the items a user has are mostly page types, such as GitLab MR (page). If only summary types reported status, M5 would show no badges at all.

Two of the attention reasons in [item types](../item-types.md) are events, not states: "your merge request was approved" and "the Jira issue's status changed". An event needs a point at which it stops needing attention, or the badge stays until the merge request is merged or the issue closes.

## Decision

- **Status belongs to what the item shows, not to how it shows it.** A type reports status when its configuration names one object on a server: GitLab MR, Issue and CI Pipeline, as summary or page, and Jira Issue, as summary or page. The summary and page types of one object share the same requests through the cache. List types report no status in M5; M6 decides what a list reports.
- **A reason has a key.** Each attention reason has a key that is stable while the reason is the same, such as `approved` or `status:10001` (the Jira status id).
- **Reasons that clear when seen.** A reason marked `clearsOnView` stops counting once the user has seen it: when a cell showing the item is visible in the active window for two seconds. The core then records the reason's key as seen for that item. A later reason with another key, such as a new Jira status, counts again.
- **No attention on the first look.** When an item has no record yet, its first status records the keys of all its `clearsOnView` reasons as seen. An issue that was already "In Review" when it was added does not count as changed.
- **States do not clear.** "Pipeline failed", "Review requested" and "Has conflicts" count for as long as they are true.
- **The seen keys are kept on the device**, in `DeviceSettings`, under the workspace id and the item id. They are not synced.

## Alternatives

- **Status only for summary types.** Simple, but no badges until M6, and a user who prefers pages would never get any.
- **A "Mark as seen" button on each badge.** Explicit, but one more click for something that looking at the item already says.
- **Sync the seen keys.** Seeing an item on one device would clear it on the other. But it writes synced files every time the user looks at an item, and makes conflict copies more likely.
- **Compare with the previous fetch instead of a seen record.** A status change would count only until the next poll, two minutes later, whether or not the user looked.

## Consequences

- Page items get badges in their cell header, the sidebar and Focus from M5.
- Each device shows a changed Jira status once, until the item is seen there.
- An item never shown in a cell, only polled because its task is Urgent or High, keeps its `clearsOnView` reasons until it is opened.
