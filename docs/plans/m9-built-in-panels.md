# M9 code plan: built-in panels

This plan builds [milestone M9](../roadmap.md#m9-built-in-panels). After M9, a task can hold a built-in panel: an HTML page shipped with the app, served from `dash://` in the app profile, that shows the status cache through the `dash` bridge. Two panels exist: Task summary and Group overview. See [web views](../web-views.md#built-in-panels) and [item types](../item-types.md#built-in-panels).

## Decisions

All accepted:

- [Decision 0094](../decisions/0094-built-in-panels-as-a-core-item-type.md): `dash.panel` is one core item type, registered as the built-in module `dash`, with a choice of panel. A panel sees only its own task and the task's group. The app profile loads only `dash:` addresses.
- [Decision 0095](../decisions/0095-the-bridge-gives-texts-and-colours.md): the bridge gives panels their translated texts, formatted values and the app's colours. It gains `openTask()`, `texts()` and `palette()`.

Smaller choices made here:

- **The scheme.** `dash` is registered with `QWebEngineUrlScheme` before `QApplication`, with host syntax and the flags `SecureScheme | LocalScheme`, so a panel is a secure context that pages of other schemes cannot load. Only the app profile installs a handler. Every other profile fails a `dash:` address.
- **Addresses.** The host must be `panels`. Every part of the path must be a plain name: no empty part, no `.` and no `..`, also percent-encoded. `dash://panels/<path>` is served from the resource `:/panels/<path>`. The one exception is `dash://panels/common/qwebchannel.js`, served from Qt's own `:/qtwebchannel/qwebchannel.js`, because the policy below forbids `qrc:`. Anything else fails with `UrlNotFound`. Only `GET` is answered; other methods fail with `RequestDenied`.
- **The headers.** Every reply sets, with `QWebEngineUrlRequestJob::setAdditionalResponseHeaders()` (Qt 6.7 and later):

  ```text
  Content-Security-Policy: default-src dash:; connect-src 'none'; object-src 'none'; base-uri 'none'; form-action 'none'; frame-ancestors 'none'
  X-Content-Type-Options: nosniff
  Referrer-Policy: no-referrer
  Cache-Control: no-store
  ```

  The content type comes from the file's suffix: `html`, `js`, `css`, `svg`. The extra directives close what `default-src` does not cover. `no-store` makes a panel changed in a new build load fresh.
- **The view.** `DashView` is a `QWebEngineView` with a `DashPage` in the app profile. It has no toolbar and no context menu. `DashPage` accepts navigation only to `dash:` addresses, makes no new windows, and denies every permission request. The view has a `QWebChannel` with the bridge registered as `dash`.
- **The bridge's data.** All members return JSON objects or lists. Shapes:

  ```text
  currentTask()     { id, title, project, group, groupId, priority, priorityName, state,
                      items: [ { id, name, type, typeName, hasStatus } ] }
  itemStatus(id)    { state, summary, message, closed, updated, attention: [ text ],
                      needsAttention }
  groupTasks(id)    [ { id, title, project, priority, priorityName, attention, attentionText, current } ]
  texts()           { key: text }
  palette()         { window, base, alternateBase, text, muted, border, accent, highlight, link,
                      attention, priorityUrgent, priorityHigh, priorityNormal, priorityLow,
                      fontFamily, fontSize }
  ```

  `state` is the `ItemStatus::State` in lowercase, such as `ok` or `notoken`. `updated` is "5 minutes ago", or empty. `groupTasks` lists active tasks not in the trash, by priority, then in sidebar order. `attention` there is `Attention::taskCount()`. Nothing returned holds a token, a server address or a profile.
- **`changed`.** One signal, emitted at most once per pass of the event loop, after any of: a `Model` change to the task, its project or group, or any task of the group; `StatusCache::statusChanged` for an item of the task; `Attention::countsChanged` when the attention the panel shows differs, since counts change for the whole workspace; a palette change of the view. Panels fetch everything again on `changed`. The data is small.
- **Opening.** `openItem(id)` opens the item alone in the preview, as `MainWindow::openItem()` does. `openTask(id)` opens the task's default dashboard in the preview, as the quick switcher does. Both work only for ids the panel may see.
- **The panels' look.** Plain HTML, CSS and JavaScript, with no framework and no build step. Each panel has `index.html`, `panel.js` and `panel.css`, and shares `common/dash.js`, which connects the channel, fetches the texts and colours, sets the CSS custom properties and calls the panel's `render()` on start and on `changed`. Text uses the system UI font at the app's size.

## Changes to the architecture

- **New `web::DashSchemeHandler`**, a `QWebEngineUrlSchemeHandler`, with a pure function `dash::resourceFor(const QUrl &)` that maps an address to a resource path or nothing. Also `dash::registerScheme()`, which `main.cpp` calls first.
- **`ProfileManager::appProfile()`** installs the handler when it makes the profile.
- **New `web::DashPage` and `web::DashView`**, in the web library, since they need no status.
- **New `items::DashBridge`**, a `QObject` with the members above. It sits in the items library, not in web as [architecture](../architecture.md#web) said before M9, because it needs the status cache and attention from `api`, and `web` does not depend on `api`. The architecture table moves it.
- **New `items::DashPanelType`**, the `dash.panel` item type, and `registerDashModule()`, which `main.cpp` calls before `registerEnabledModules()`.
- **`ItemTypeRegistry`** gains `registerBuiltInModule()` and `isBuiltIn(moduleId)`.
- **`ItemContextImpl`** gains `createDashPanel(panel, parent)`, which `DashPanelType::createView()` reaches with a `dynamic_cast`. Without the services it needs, it returns a message view.
- **`PlacementServices`** gains `openItem` and `openTask` callbacks. `MainWindow` sets them.
- **`ModulesDialog`** shows built-in modules first, under "Built in".
- **New `resources/panels/`**, compiled into the app with `qt_add_resources`.

## Part 1: the scheme

- `dash::registerScheme()` in `main.cpp`, before `QApplication`.
- `DashSchemeHandler` and `resourceFor()`.
- `ProfileManager::appProfile()` installs the handler.
- Tests, in `tst_dash_scheme`, which registers the scheme in its own `main()`. Its pages come from `tests/web/dash/`, compiled into the test as `dash://panels/test/`:
  - `resourceFor()`: a good path; the wrong host; `..` in several spellings, also percent-encoded; an empty path; a folder; `qwebchannel.js`.
  - The content types and the headers, as plain values.
  - A page in the app profile loads `dash://panels/test/index.html`, with its script and `qwebchannel.js`. Its inline script does not run, which shows that the policy arrived. A page in a list profile fails to load the same address.
  - The page tests need release libraries; see [debug builds](../web-views.md#debug-builds). In debug they are skipped, like `tst_page_keys`.

## Part 2: the type and the view

- `ItemTypeRegistry::registerBuiltInModule()` and `isBuiltIn()`.
- `DashPanelType`: id `dash.panel`, name "Built-in panel", short name "Panel", the field `panel`, `Rendering::Web`, `hasStatus()` false. The icon is a new `resources/icons/panel.svg`.
- `registerDashModule()`: module name "Built in", with a UUID, version, author and homepage like the other modules.
- `DashPage` and `DashView`.
- `ItemContextImpl::createDashPanel()`: a `DashView` with the channel and a `DashBridge`, loading `dash://panels/<panel>/index.html`. An unknown panel value shows a message instead.
- `ModulesDialog`: the "Built in" section.
- `ItemDialog` offers no profile and no page keys for a built-in type.
- Tests: `isBuiltIn()`; the type's field; the messages for an unknown panel, for a context that is not the core's, and without a profile manager, in `tst_dash_panel`. In `tst_dash_scheme`: `DashPage` accepts only `dash:` addresses and makes no windows, called directly; and a script in a loaded panel cannot leave `dash:` (release only).

## Part 3: the bridge

- `DashBridge`, made with the placement's services and its task id.
- `PlacementServices::openItem` and `openTask`; `MainWindow` sets them.
- Tests, in `tst_dash_bridge`, with a `Model` from fixtures, a `StatusCache` fed with statuses by hand, and `Attention`, as the M5 and M8 tests do:
  - `currentTask()` lists the items with `hasStatus` set from their types;
  - `itemStatus()` for an item of the task, for an item of another task (empty), and for each state;
  - `groupTasks()` for the task's group, sorted, without Done and trashed tasks; for another group (empty);
  - `changed` fires once for several changes in one pass, and not for a change to an unrelated task;
  - `texts()` holds every key the panels use; this list lives in the test and in the bridge;
  - `palette()` follows a palette set on the application;
  - `openItem` and `openTask` call the callbacks only for ids the panel may see.

## Part 4: the panels

- `common/dash.js` and `common/dash.css`.
- **Task summary.** One row per item with status, in the task's item order. Each row: the type name as a small pill, the item name, the summary, and the time it was updated. A row that needs attention has a red left edge and the reasons under it. A row in an error state, such as `notoken`, shows the message in muted text. A click on a row opens the item. Without any item with status, it shows "No items in this task report a status."
- **Group overview.** A heading with the group's name. One row per active task of the group: the priority marker, the title, the project in muted text, and the attention badge when the count is not zero. A click opens the task. Without tasks, it shows "No active tasks in this group."
- Both lay out in one column and stay readable in a cell 300 pixels wide.
- Tests: a page loads each panel with a bridge over fixture data, and checks the number of rows and the text of the first one with `runJavaScript()` (release only). The look is checked by hand.

## Part 5: documents

In the same commits as the code:

- [web views](../web-views.md#built-in-panels): the scheme's flags and addresses, the headers, navigation, the new bridge members and the data shapes.
- [item types](../item-types.md#built-in-panels): the field `panel`, and what each panel shows.
- [architecture](../architecture.md): the new classes, and `DashBridge` in the items library.
- [modules](../modules.md) and [ui](../ui.md#modules-window): the built-in module, and the Modules window's "Built in" section.
- [decision 0056](../decisions/0056-modules-window.md) is not rewritten; it already expects the section.
- The decision index, and the roadmap boxes once the user has checked.

## Order of work

Parts 1 and 2 touch different files, and Part 4 needs only the shapes above, so they could run in parallel. They are small, though, and Part 4 is best written against a running bridge, so one session does them in order, each on its own branch, merged when its build and tests pass.

| Order | Branch | Parts |
|---|---|---|
| 1 | `m9-dash-scheme` | Part 1 |
| 2 | `m9-dash-type` | Part 2 |
| 3 | `m9-dash-bridge` | Part 3 |
| 4 | `m9-panels` | Part 4 |

Part 5 goes with each part.

## Checking

- **Automatic:** the tests above, by CTest locally and in CI on Windows and Linux, with all modules and with none. The page tests run in the release build only.
- **By hand on Windows,** listed in `docs/plans/m9-checks.md`, written with Part 4:
  - adding a Task summary and a Group overview from the type picker, and changing the panel in the item dialog;
  - Task summary with a GitLab item and a Jira item, before and after a status changes, and with a missing token;
  - Group overview as tasks change priority, are marked Done, move to the trash and gain attention;
  - a click opens the item or the task;
  - the panels in light and dark, switching while the app runs, and in Brazilian Portuguese;
  - a panel unloaded by the memory policy and loaded again;
  - Help > Modules shows "Built in".
- **Not checked by hand:** macOS and Linux. Nothing in M9 is platform code. The user tries the panels at the next manual macOS build.

## Out of scope

- Panels on root dashboards that show a whole workspace, such as all tasks that need attention. See decision 0094.
- Panels from modules. Modules keep building views with `ItemContext`.
- Panels that change data, such as setting a task's priority. The bridge only reads and opens.
