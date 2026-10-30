# M6 code plan: native item types

This plan implements [milestone M6](../roadmap.md#m6-native-item-types). After M6, the GitLab MR, CI Pipeline and Issue types and the Jira Issue type have native summaries, the three list types have tables with a preview, and the summaries offer the light actions. Pasting a link is already done.

## Decisions

- [Decision 0066](../decisions/0066-fixed-native-layouts-with-sections.md), accepted: fixed layouts, with sections that can be hidden per item.
- [Decision 0067](../decisions/0067-lists-report-status.md), proposed: lists report status with their rows and no attention. The view fills the merge request list's pipeline and approvals columns for the first 25 rows.
- [Decision 0068](../decisions/0068-views-read-what-the-user-asked-for.md), proposed: an item's own data comes from the status cache, and a view asks the server directly only for what a user's action or a visible list needs.
- [Decision 0069](../decisions/0069-open-page-in-the-placement.md), proposed: "Open page" is shown by the placement, through `ItemContext::showPage()`.

Smaller choices made here:

- **Project settings stay.** The M5 plan left this to M6. GitLab's `project` is the default of every item and of list scopes. Jira's `projectKey` is now used by two list presets. Both earn their place.
- **Colours are fixed in M6.** All colours of status pills come from one function in the view kit, so a later theme has one place to change. See [ideas for later](#ideas-for-later).
- **Confirmation.** Only cancelling a pipeline asks first. Approving, retrying, commenting, closing and moving an issue do not, because the user can see the result and can undo or repeat it.
- **Close and reopen** are in [item types](../item-types.md#gitlab-issue-summary-gitlab-issue-page-gitlab-issue-list) but missing from the roadmap and the client. M6 adds them, which needs `ApiClient::put()`.

## Changes to the architecture

- **`ApiClient::put()`.** GitLab closes and reopens an issue with `PUT`. A `403` on a `PUT` is treated like one on a `POST`: only that action fails.
- **A view kit for modules.** The `gitlab` and `jira` modules need the same small widgets and cannot depend on each other. A new static library, `mirante_module_widgets`, holds them under `<mirante/widgets/...>`. `mirante::module_api` links it. It uses only Qt and the module interface, never core headers, so the rule that modules see only the module interface still holds.
- **Two new `ItemContext` members.** `showPage(url)`, as in decision 0069, and `addItemToTask(typeId, name, config)`, for "Add to task as item". Both have default implementations, so existing fake contexts need no change.
- **Views depend on status.** `createView()` of a summary subscribes to `ItemContext::status()`, and draws from `ItemStatus::data`. Where there is no status source, as in tests with a plain context, the view draws from what it is given.

## Part 1: the module interface

```cpp
// ApiClient.h
virtual void put(const QString &path, const QJsonDocument &body, QObject *receiver,
                 std::function<void(const ApiReply &)> done) = 0;

// ItemContext.h
// Shows a web page in place of the view, in the item's profile, with a way
// back. Does nothing where the core has no web views, as in tests. See
// decision 0069.
virtual void showPage(const QUrl &url) { Q_UNUSED(url) }
// Adds an item to the task this item belongs to, and returns its id, or an
// empty string if it could not. No dashboard changes.
virtual QString addItemToTask(const QString &typeId, const QString &name, const QJsonObject &config)
{
    Q_UNUSED(typeId) Q_UNUSED(name) Q_UNUSED(config)
    return {};
}
```

`FakeApiClient` gains `put()`, recorded with the method `"PUT"`.

## Part 2: the view kit

`src/module_api/widgets/`, one header and one source file for each:

| Class | What it does |
|---|---|
| `StatusPill` | A small rounded label, with a `StatusRole`: `Good`, `Bad`, `Waiting` or `Neutral`. `statusColours(role, palette)` gives the fill and text, with a light and a dark pair, chosen by the palette's window lightness. These are the Standard colours. |
| `FlowRow` | A `QLayout` that wraps its widgets, for pills, chips and action buttons. |
| `SummaryView` | A `QScrollArea` with one column: a title, a line of pills and text, an `ActionRow`, then sections. `setSection(key, title, widget)` adds or replaces a section in the order of first use, and `setSectionVisible(key, on)` hides it. The scroll position is kept across updates. |
| `ActionRow` | A `FlowRow` of buttons. `addAction(id, label, shortLabel, handler)` adds one, `setActionVisible(id, on)` shows or hides it, and `setBusy(id, on)` disables it while a request runs. It has a line for an error message, and `showError(text)` fills it. In a narrow cell it uses the short labels. |
| `CommentDialog` | `askComment(parent, title)` returns the text, or nothing if cancelled. A plain multi-line box. Empty text cannot be sent. |
| `ItemListView` | A `QTableView`, a sort proxy, a splitter and a preview area. `setColumns()` gives each column a title, a priority for hiding in a narrow cell, and whether it stretches. `setRows()` takes `ListRow`s: per column a text, a sort key and an optional `StatusRole`. It keeps the selection by a row's key. `setPreview(widget)` shows the preview under the splitter, with a close button. Signals: `rowSelected(key)`, and `rowMenuRequested(key, globalPos)`. |

The columns hide by priority as the width falls. The title column always stays. Sorting starts on the last update column, newest first.

**Tests**, in `tests/module_widgets/`: `tst_flow_row` (wrapping), `tst_summary_view` (order, replacing, hiding, scroll kept), `tst_action_row` (busy, error, short labels), `tst_item_list_view` (sorting by key, selection kept across `setRows`, hiding by width, preview hides and returns), `tst_status_pill` (a colour pair for each role in both modes, with enough contrast).

## Part 3: the core

- **`ServerClient`:** `put()`, through the same queue, without ETags. `tst_server_client` additions: the method and body, and a `403` that does not pause the server.
- **`Placement`:** `showPage()`. It holds the page in a second pointer, hides the view, and adds the page to the `ViewPool`. The header widget is a small container with "Back to summary" and the page's toolbar. Back, a pool unload, or a new view drops the page. The view is not made again. `tst_placement` additions: a page shows, back returns the same view object, an unload returns the view, and a change of configuration drops the page.
- **`ItemContextImpl`:** `showPage()` and `addItemToTask()`, using `edits::addItem` and `Model::setTask`. The new item's profile is empty, so it follows the group. `tst_placement` additions for the second.
- **`StatusCache`:** nothing changes. List types return true from `hasStatus()`.

## Part 4: the gitlab module, client and status

**`GitLabClient`** gains:

- `projectIssues` and `groupIssues`, like the merge request lists;
- `issueNotes(project, iid, maxRows)`: `GET .../issues/<iid>/notes?sort=desc&order_by=created_at`;
- `setIssueState(project, iid, "close" | "reopen")`: `PUT .../issues/<iid>` with `{"state_event": ...}`;
- `pipeline(project, id)`, for the preview of a run.

**Status:**

| Type | Requests beyond M5 | `data` beyond M5 |
|---|---|---|
| MR summary | With a failed head pipeline, its jobs | `jobs` |
| MR page | none | none |
| Issue summary | The latest notes, up to 20, without system notes | `notes` |
| Issue page | none | none |
| MR List | The list; the current user for `reviewRequested` | `rows`, `total` |
| Issue List | The list | `rows`, `total` |

So summary and page types no longer share one fetch. The attention rules do not change.

**List queries,** in a pure function `listQuery(kind, preset, userId, filter)`:

| Preset | Query |
|---|---|
| `createdByMe` | `state=opened&scope=created_by_me` |
| `assignedToMe` | `state=opened&scope=assigned_to_me` |
| `reviewRequested` | `state=opened&scope=all&reviewer_id=<me>` |
| `allOpen` | `state=opened&scope=all` |

Each adds `order_by=updated_at`. The custom filter's parameters are added last and replace those of the same name. An unusable scope, such as `group` without a group path, reports `State::None` with a message, as an incomplete item does in M5.

The summary reads like "12 merge requests", or "100+ merge requests" at the row limit.

**Tests:** `tst_client` additions; `tst_status` additions on new recorded responses (jobs only when failed, system notes left out, the page's requests unchanged, the list's status, `reviewRequested` asking for the user, no attention, `closed` unset); a new `tst_list_query`.

## Part 5: the gitlab module, views

Each view is a file with one factory function, written as a stub by T0.

**`MergeRequestView`:**

- **Top:** title; pills for state and draft; `source → target`; actions.
- **Actions:** Approve, shown while open and not yet approved by me. Retry pipeline, shown when there is a head pipeline that failed or was cancelled. Comment. Open page.
- **Sections and fields:** Pipeline (`showPipeline`) with the status, id and failed jobs, each with its duration and a log link. Approvals (`showApprovals`) with the count and who. People (`showPeople`) with author, assignees and reviewers. Labels (`showLabels`).
- The widget has a second factory, `createMergeRequestPreview`, which takes a `GitLabClient` and the JSON directly, and lists use it. "Open page" is the same in both.

**`PipelineView`:** a top with the status, branch and duration; actions Retry pipeline, Cancel (with a confirmation) and Open page. Then one section per stage, in the order of the jobs, listing jobs with their status, duration and a Retry button on failed jobs. A log link on each failed job opens its `web_url`. "No pipeline on main" and the main folder's message show as a message.

**`IssueView`:** a top with title, state pill and actions: Comment, Close or Reopen, Open page. Sections: Details (`showDetails`: assignees, milestone, due date), Labels (`showLabels`), Comments (`showComments`: the latest five, newest first, as plain text with author and date).

**`MergeRequestListView`:** columns Ref, Title, Author, Pipeline, Approvals and Updated, in that order. Fields `showPipeline` and `showApprovals`, on by default. After each new status, while visible, it asks for the first 25 rows' merge request and approvals, as decision 0067 describes. Clicking a row loads its merge request and approvals and shows the preview. The row menu has "Add to task as item", "Open page" and "Copy link".

**`IssueListView`:** columns Ref, Title, Assignees, Labels and Updated. The preview and menu follow the same pattern.

**Every view:**

- With no data yet, it shows "Loading…", or the status's message. Problems stay in the cell header.
- With old data and an error, it keeps showing the data.
- A link opens in the external browser. "Open page" calls `showPage()`, or the external browser with `Shift`.
- A failed action shows `reply.message` in the action row. A `403` says that the token cannot do this, and names the `api` scope.
- Dates are local and short. Everything is plain text, so nothing from the server is read as markup.

**Tests:** `tst_views`, with a fake context and fake status source: each view draws from recorded data; each section's field hides it; the action buttons follow the rules above and post the right request; a success refreshes the status; a failure shows the message; `Shift` does not call `showPage()`; the list's further requests stop at 25 rows and while hidden; "Add to task as item" passes the right type and configuration.

## Part 6: the jira module

**`JiraClient`** needs nothing new, except that `issue()` may ask for more `expand` values.

**Status:**

- **The summary** asks for all fields, with `expand=renderedFields,names`. `data` gains `issue`, the answer. The sprint is the field whose entry in `names` is "Sprint", because its id differs per site. The page keeps its six fields.
- **The list** asks `search()` with `summary`, `status`, `assignee`, `updated`, and reports `rows` and `total`.

**List queries,** in a pure function `listJql(preset, jql, projectKey)`:

| Preset | JQL |
|---|---|
| `myOpen` | `assignee = currentUser() AND resolution = Unresolved ORDER BY updated DESC` |
| `currentSprint` | `project = "<key>" AND sprint in openSprints() ORDER BY rank` |
| `recentlyUpdated` | `project = "<key>" AND updated >= -7d ORDER BY updated DESC` |
| `jql` | the `jql` field |

Without a project key, the two project presets report `State::None` with "Set the Jira project key in the project's settings." The key is quoted, so a value cannot change the query.

**`IssueView`:**

- **Top:** key and summary; pills for status and type; actions.
- **Actions:** Change status, a drop-down filled from `GET transitions` when opened; Comment; Open page.
- **Sections and fields:** Details (`showDetails`: priority, assignee, reporter, sprint), Description (`showDescription`: the HTML in `renderedFields`, in a `QTextBrowser`), Comments (`showComments`: the latest five), and Sub-tasks and links (`showLinks`).
- The `QTextBrowser` does not load remote resources, so a page cannot call out or show tracking images, and it opens links in the external browser, resolved against the base URL.
- A second factory gives the preview for a list.

**`IssueListView`:** columns Key, Summary, Status, Assignee and Updated. The status has a pill, with its role from the status category. The preview and menu follow the merge request list.

**Tests:** `tst_status` additions on both deployments; a new `tst_list_jql` with a key that holds a quote; `tst_views` as for GitLab, including the transitions and a description with a remote image that is not fetched.

## Part 7: documents

Each task updates what it changes. T0 updates [modules](../modules.md) for the two `ItemContext` members, `put()` and the view kit, and [integrations](../integrations.md#polling-and-caching) for decision 0068. T4 to T8 update [item types](../item-types.md) with the layouts and fields, [integrations](../integrations.md) with the new endpoints, and each module's `README.md`. The sentences "Lists report no status in M5" go.

## Tasks for parallel work

The rules, the roles and the brief are those of the [M2 plan](m2-app-skeleton.md#tasks-for-parallel-work), with `m2-app-skeleton.md` replaced by this plan. At most three agents run at the same time, as for M5. The headers written by T0 are the interfaces. Where they differ in detail from the sketches above, the headers win.

| Task | Branch | Work | Owns | Needs |
|---|---|---|---|---|
| T0 | `m6-interfaces` | Part 1. Every header of Part 2 and its library, with stubs. Stub files and factories for every view in parts 5 and 6, each module's `CMakeLists.txt`, and the types wired to the factories. The new client method declarations. Lists keep `hasStatus()` false until T4 and T7. Documents as in Part 7. | everything it creates; after merging, the headers belong to the orchestrator | none |
| T1 | `m6-view-kit` | Part 2. | `src/module_api/widgets/`, `tests/module_widgets/` | T0 |
| T2 | `m6-core` | Part 3. | `src/api/ServerClient.cpp`, `src/items/Placement.cpp`, `src/items/ItemContextImpl.cpp`, their tests | T0 |
| T3 | `m6-gitlab-status` | Part 4, without list status. Merges first of the GitLab tasks, because T5 and T6 read its `data`. | `GitLabClient.cpp`, `GitLabStatus.cpp`, `tests/tst_client.cpp`, `tests/tst_status.cpp`, `tests/data/` | T0 |
| T4 | `m6-gitlab-lists` | Part 4's list status and queries. | `GitLabLists.cpp`, `tests/tst_list_query.cpp` | T3 |
| T5 | `m6-gitlab-summaries` | Part 5: `MergeRequestView`, `PipelineView`, `IssueView`. | those views, `tests/tst_summary_views.cpp` | T1, T2, T3 |
| T6 | `m6-gitlab-list-views` | Part 5: the two list views. | those views, `tests/tst_list_views.cpp` | T1, T2, T4, T5 |
| T7 | `m6-jira` | Part 6. | `src/modules/jira/` | T1, T2 |
| T8 | none | Done by the orchestrator: write `docs/plans/m6-checks.md`, run the `dev`, `no-modules`, `release` and `ci` presets, and tick the roadmap once the user has checked. | any | all |

### Waves

| Wave | Tasks | At the same time |
|---|---|---|
| 0 | T0, then the user reviews the headers | 1 |
| 1 | T1, T2, T3 | 3 |
| 2 | T4, T5, T7 | 3 |
| 3 | T6 | 1 |
| 4 | T8 | 1 |

## Checking

- **Automatic:** the tests above, by CTest locally and in CI on three platforms, with all modules and with none. No test uses the network or the real keychain.
- **By hand on Windows,** listed in `docs/plans/m6-checks.md` by T8, against gitlab.com and the user's Jira Cloud site:
  - each summary and list shows real data, in a wide and a narrow cell;
  - each section's field hides it;
  - approve, retry a pipeline, retry a job, cancel, comment, close and reopen on a scratch project; a status change and a comment on a scratch issue;
  - a `read_api` token shows a clear message on an action, and the server keeps working;
  - "Open page" and the way back, `Shift` for the browser, and a page unloaded by the memory policy;
  - a row's preview, and "Add to task as item";
  - a Jira description with an image and a link;
  - a Jira Data Center site, if one is at hand; otherwise noted as not checked.

## Out of scope

- The CI Pipeline following the main folder's branch: M7.
- Jira development information (linked branches and merge requests). It comes from a less stable API. [Item types](../item-types.md#jira-issue-summary) already allows leaving it out quietly, so the section does not appear in M6.
- Suggesting "done" for a task when its items are closed. The M3 plan left this to M6, but it is not in M6's boxes in the roadmap, and it needs its own place in the interface. The status data it needs is there from M5. It goes with M8's notifications unless the user wants it sooner.
- Desktop notifications and the tray: M8.
- Remembering collapsed sections, column order and column widths: they need the device settings for items of M7. See [decision 0066](../decisions/0066-fixed-native-layouts-with-sections.md).

## Ideas for later

- **Themes and item colours.** The user would like simple colour choices, such as a background colour or a theme, but not in M6. The status pills' colours are in one function of the view kit, and the other colours follow the Qt palette, so a later theme has few places to change.
- **Attention for new rows in a list,** as [decision 0067](../decisions/0067-lists-report-status.md) mentions.
