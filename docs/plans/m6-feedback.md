# M6 feedback plan

The user tried M6 and asked for these changes. They replace parts of [the M6 plan](m6-native-item-types.md). See decisions [0070](../decisions/0070-read-mostly-native-views.md), [0071](../decisions/0071-lists-link-out-without-a-preview.md), [0072](../decisions/0072-pipelines-started-together.md) and [0073](../decisions/0073-jira-extra-fields.md).

## Core (U)

- **Tabs.** Every tab in the main window's tab row can be reordered by dragging, root dashboards among them, and the order is kept as the existing tab order is. Focus and the preview keep their fixed slots. Tabs inside a dashboard's layout can be dragged within their bar too, while the dashboard is unlocked.
- **Project dialog.** A module's section shows only if the project has values for it. An "Add settings" button lists the other modules' sections, and adding one shows it. Stored values are never lost.
- **Profile choice.** The item dialog's first entry reads "Work (from the group)", and the type's other uses follow.

## View kit (K)

- `ItemListView` loses the splitter, the preview, `setPreview()`, `preview()` and `rowSelected`.
- `ListCell` gains `links`, each with an id and a label. A cell draws them as links, and a click emits `rowLinkActivated(key, linkId)`.
- `setPageSize(n)`, with Previous and Next and a line such as "1-50 of 100". Sorting and filtering cover all rows, and the page follows. `visibleKeys()` gives the current page's keys in table order, so views need no reach into the table.
- `addFilter(column, label)`: a drop-down above the table, filled from that column's values, with "All" first. The filter survives `setRows()`.
- `compactDate(QDateTime)`: `10-30 14:32` in this year, `2025-10-30` otherwise, in local time.

## GitLab, summaries and pipelines (G1)

- **Remove** approve, comment, close and reopen from views, client and tests.
- **MR summary.** The Pipeline section links to the pipeline's page, with "Web" on its row.
- **CI Pipeline view.** Jobs in a table, grouped by stage: a status pill, the name, the duration, then links: "Log" on any job that has run, "Retry" on a finished job, "Play" on a manual one. The top has Retry pipeline, Cancel, "Trigger pipeline", "Open page" and "Web", and a link to the merge request when there is one: from the configuration for `mergeRequest`, otherwise the open merge request of that source branch.
- **The group,** Previous and Next, and the refresh every 15 seconds, as decision 0072 says. Attention follows it.
- **Default branch.** The `branch` field defaults to `master`, also as its placeholder. An address without `ref` takes that default.
- **Client:** `pipelines(project, query, maxRows)`, `mergeRequestPipelines`, `playJob`, `createPipeline(project, ref)`. An MR's `source_branch` lookup goes through `projectMergeRequests`.

## GitLab, lists (G2)

- MR List and Issue List use the row links and pages, with the item fields `limit` and `pageSize`. The preview factories go. Compact dates.
- **New type:** `gitlab.pipeline.list`, "GitLab CI Pipeline List". Fields: `project`; `source`, one of `all`, `mine`, `branch` or `mergeRequest`; `branch`; `iid`; `limit`; `pageSize`. Columns: Id, Status (pill), Ref, Author, Started, then links. Add creates a `gitlab.pipeline` item for its branch. `fromUrl` takes `<project>/-/pipelines` only for the list when it has no `ref`. Status reports the row count and no attention.

## Jira (J)

- **Remove** change status and comment from views, client and tests.
- **Summary.** Children: the issues whose parent is this one, from `search` with `parent = <key>`, or also `"Epic Link" = <key>` on Data Center, shown in Sub-tasks and links with their status pills, each linking to the web. An epic can have many, so up to 50 rows show, with a count of the rest.
- **List.** Row links, pages, the item fields `limit` and `pageSize`, the Status filter, compact dates. The previews go.
- **Extra fields,** as decision 0073 says.

## Tasks

At most three agents run at once. Each works in its own worktree and does not merge.

| Task | Branch | Needs |
|---|---|---|
| K | `m6f-view-kit` | none |
| U | `m6f-core` | none |
| G1 | `m6f-gitlab-pipelines` | none in the kit; it may start beside K |
| J | `m6f-jira` | K |
| G2 | `m6f-gitlab-lists` | K, G1 |

## Out of scope

- Saved filters and saved column widths.
- Server-side paging.
