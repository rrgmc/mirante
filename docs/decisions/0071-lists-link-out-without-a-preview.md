# 0071. Lists link out from each row, without a preview

- Status: Accepted
- Date: 2026-10-30

## Context

[Decision 0066](0066-fixed-native-layouts-with-sections.md) gave lists a preview of the selected row under a splitter. After trying it, the user prefers each row to carry links, with the web page first. A table also needs more rows on screen, a way past its first rows, and simple filters.

## Decision

- **No preview.** The splitter, the preview and the requests behind it go. This supersedes those parts of decisions 0066 and [0068](0068-views-read-what-the-user-asked-for.md).
- **Links on each row,** in a column that never hides: "Web" first, then "Open" (the page in the cell) and "Add" (add to the task as an item). The row menu offers the same.
- **Pages.** An item's `limit` field sets how many rows it fetches, 100 by default, and its `pageSize` field how many the view shows at a time, 50 by default, with Previous and Next. The fetched rows are sorted and filtered together.
- **Filters in the view.** A list can offer a drop-down for a column, filled from the fetched rows. Jira List offers Status. The choice is not saved.
- **Compact dates.** Dates read `10-30 14:32`, or `2025-10-30` for another year. They sort by the real date.

## Alternatives

- **Ask the server for one page at a time.** Less to fetch, but sorting and filtering would then see only that page, and the status would need a page of its own.
- **Save the filter.** It would write a synced file for a passing choice, as decision 0066 says of collapsed sections.

## Consequences

- A filter or a sort sees only the fetched rows, so a raised `limit` costs more on each poll.
- The first 25 rows of a merge request list that fill their pipeline and approvals are the first 25 of the current page.
