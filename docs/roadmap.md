# Roadmap

The milestones are in order. Each one ends in a working state and a commit. A detailed code plan for a milestone is written just before it starts, not earlier.

Each milestone has acceptance criteria. Development and checking happen on Windows. From M2 onwards, every milestone must also build and pass its tests on macOS and Linux, checked by the CI build added in M2.

## M0: Design documents

The documents in this folder.

- [x] Design, architecture, roadmap and decision records are written and committed.

## M1: HTML prototype

A clickable prototype of the navigation, made of one HTML page with mock data. Web pages and API data are shown as placeholders. See [decision 0012](decisions/0012-html-prototype-first.md).

- [x] The sidebar shows groups, projects and tasks, sorted by priority, with markers and dimmed Low items.
- [x] Clicking a task opens it in a preview tab, and pinning works.
- [x] Root dashboards and task dashboards show nested splits and tabs.
- [x] Layouts can be edited with the context menu and with drag and drop onto edges and centres.
- [x] Detaching is simulated.
- [x] The Focus view and the `Ctrl+K` switcher work on the mock data.
- [x] The user has tried it, and [ui.md](ui.md) is updated with the changes agreed.

## M2: App skeleton and layouts

The base of the Qt app, reading a fixed set of JSON files. Nothing is written back. There are no web views and no API calls. See [decision 0015](decisions/0015-app-skeleton-instead-of-spike.md). The open boxes are checked by hand with [the M2 checklist](plans/m2-checks.md).

- [x] CMake project with Qt 6.8, qtkeychain and QHotkey. It builds on Windows, macOS and Linux.
- [x] A GitHub Actions build for Windows, macOS and Linux runs the tests with all modules on, and a second build has all modules off.
- [x] The [module](modules.md) mechanism: `mirante_add_module()`, one option per module, `MIRANTE_MODULES`, the `DEPENDS` check and the generated `enabled_modules.cpp`.
- [x] The module interface target `mirante::module_api`. Modules cannot include other core headers.
- [x] One folder per module in `src/modules/`, each registering its item types with id, name and icon only.
- [x] A build with all modules off and a build with all on both succeed.
- [x] `core` loads the fixture files into `Model`.
- [ ] The sidebar, root tab bar and preview tab work as in [ui](ui.md).
- [ ] `LayoutView` shows any layout tree, with placeholder cells that show the item type and name.
- [ ] Items whose module is not in the build show the missing-module placeholder.
- [ ] Layout editing works with the context menu and with drag and drop, including normalisation. Changes are kept in memory only.
- [ ] `Ctrl+K` switcher and the panel focus shortcuts work.
- [x] Tests cover JSON conversion, layout tree edits and normalisation, and unchanged write-back of data from missing modules.

## M3: Data folder and editing

- [ ] The data folder can be chosen. `Store` writes changes with `QSaveFile`, and reloads external changes.
- [ ] Groups, projects, tasks, items and dashboards can be created, edited and deleted.
- [ ] Priorities, including drag across priority levels, and task states work.
- [ ] The Focus view works.
- [ ] Project folders can be mapped to a local path on each device.
- [ ] Two app instances pointed at the same folder see each other's changes. Conflict copies appear in the warning bar.
- [ ] Tests cover the Store, migrations, conflict detection and sorting.

## M4: Web views

- [ ] `ProfileManager` with group, shared and app profiles, and per-host overrides.
- [ ] Item types Web page, Claude session (web), GitLab MR (page), GitLab Issue (page) and Jira Issue (page).
- [ ] Logging in to claude.ai, gitlab.com and Jira Cloud works in a group profile and survives a restart. The result is recorded in [web views](web-views.md#known-risk-logins-in-embedded-browsers).
- [ ] A login in group A is not visible in group B.
- [ ] The same item in two visible cells shows two live views.
- [ ] The memory policy unloads hidden views beyond the limit.
- [ ] Panels can be detached and reattached, and window geometry is stored on the device.

## M5: Connections and status

- [ ] Modules register connection kinds and project settings. The connection and project dialogs are built from their fields.
- [ ] Connections can be added to groups, with defaults and project overrides.
- [ ] `SecretStore` stores tokens in the keychain on all three platforms.
- [ ] `ItemContext` gives modules connections with `authorize()` and `secret()`.
- [ ] The `gitlab` and `jira` modules' clients cover the endpoints in [integrations](integrations.md), for both Jira Cloud and Data Center.
- [ ] `StatusCache` polls, caches and computes attention.
- [ ] Attention badges appear in the sidebar, Focus and cell headers.
- [ ] Missing tokens, failed authentication and network errors are shown as described.

## M6: Native item types

- [ ] GitLab MR (summary), MR List, CI Pipeline, Issue (summary) and Issue List.
- [ ] Jira Issue (summary) and Jira List.
- [ ] Light actions: approve, retry, cancel, change Jira status, comment.
- [ ] Pasting a URL into the item dialog picks the right type and fills its fields.
- [ ] Tests cover URL recognition and the attention rules.

## M7: Quick add, Claude local, folders and notes

- [ ] Quick add from a Jira key or merge request URL, through the modules' quick add handlers and related-item finders, including the project match and related merge requests.
- [ ] Claude session (local) starts `claude --resume` in a terminal, on all three platforms.
- [ ] The Folder / terminal item shows the branch and status, and opens a terminal, editor and file manager.
- [ ] The CI Pipeline item can follow the current branch of the main folder.
- [ ] Notes item with Markdown editing and preview.

## M8: Tray, notifications and keyboard

- [ ] The tray icon with the attention count, and hiding to the tray on close. The fallback works where no tray exists.
- [ ] Desktop notification when a linked pipeline fails. Clicking it opens the task.
- [ ] The global hotkey works on Windows, macOS and X11, and reports clearly on Wayland.
- [ ] All shortcuts in [ui](ui.md#keyboard) work, including inside focused web pages.

## M9: Built-in panels

- [ ] The `dash://` scheme, served only in the app profile, with the security headers.
- [ ] `DashBridge` provides the members listed in [web views](web-views.md#built-in-panels).
- [ ] The Task summary and Group overview panels.

## M10: Packaging

- [ ] CI builds the Windows package and the macOS app bundle.
- [ ] Windows package with `windeployqt`, macOS app bundle with `macdeployqt`.
- [ ] A short install and first-run guide in the root `README.md`.
