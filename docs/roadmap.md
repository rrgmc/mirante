# Roadmap

The milestones are in order. Each one ends in a working state and a commit. A detailed code plan for a milestone is written just before it starts, not earlier.

Each milestone has acceptance criteria. Development and checking happen on Windows. From M2 onwards, every milestone must also build and pass its tests on Linux, checked by the CI build added in M2, and on macOS, which the user builds by hand from time to time during development. See [decision 0077](decisions/0077-no-macos-in-ci-during-development.md).

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
- [x] The sidebar, root tab bar and preview tab work as in [ui](ui.md).
- [x] `LayoutView` shows any layout tree, with placeholder cells that show the item type and name.
- [x] Items whose module is not in the build show the missing-module placeholder.
- [x] Layout editing works with the context menu and with drag and drop, including normalisation. Changes are kept in memory only.
- [x] `Ctrl+K` switcher and the panel focus shortcuts work.
- [x] Tests cover JSON conversion, layout tree edits and normalisation, and unchanged write-back of data from missing modules.

## M3: Data folder and editing

See [the M3 plan](plans/m3-data-folder.md).

- [x] `Store` writes changes with `QSaveFile`, and reloads external changes. (Choosing the data folder, as a workspace, arrived early in M2.)
- [x] Groups, projects, tasks, items and dashboards can be created, edited and deleted.
- [x] The `files/` folder of the data folder: pasting an image from the clipboard creates or fills an Image item and saves the image there; unused files are deleted. See [decision 0021](decisions/0021-image-items-as-files.md).
- [x] Priorities and task states work. Priority shows as a marker and the sidebar keeps the manual order; see [decision 0034](decisions/0034-manual-order-in-the-sidebar.md).
- [x] The Focus view works.
- [x] Project folders can be mapped to a local path on each device.
- [x] Two app instances pointed at the same folder see each other's changes. Conflict copies appear in the warning bar.
- [x] Tests cover the Store, migrations, conflict detection and sorting.

## M4: Web views

See [the M4 plan](plans/m4-web-views.md).

- [x] `ProfileManager` with the web profiles of the workspace and the app profile. The user manages the list; groups choose a default and web items may override it. See [decision 0042](decisions/0042-user-managed-web-profiles.md).
- [x] Item types Web page, Claude session (web), GitLab MR (page), GitLab Issue (page) and Jira Issue (page).
- [x] Logging in to claude.ai, gitlab.com and Jira Cloud works in a web profile and survives a restart. The result is recorded in [web views](web-views.md#known-risk-logins-in-embedded-browsers).
- [x] A login in one web profile is not visible in another.
- [x] The same item in two visible cells shows two live views.
- [x] The memory policy unloads hidden views beyond the limit.
- [x] Root dashboard tabs can be detached into a window and put back, and window geometry is stored on the device. Nothing else detaches. See [decision 0024](decisions/0024-detach-root-dashboard-tabs.md).

## M5: Servers and status

See [the M5 plan](plans/m5-servers-and-status.md).

- [x] Modules register project settings. The project dialog is built from their fields.
- [x] Tokens typed into the settings screens are stored with qtkeychain, under `<workspaceId>/<profileId>/<moduleId>/<fieldKey>`. See decisions [0054](decisions/0054-module-settings-replace-connections.md) and [0059](decisions/0059-logins-per-workspace.md).
- [x] `SecretStore` stores tokens in the keychain on all three platforms.
- [x] `ItemContext` gives modules their profile's server with `authorize()` and `secret()`.
- [x] The `gitlab` and `jira` modules' clients cover the endpoints in [integrations](integrations.md), for both Jira Cloud and Data Center.
- [x] `StatusCache` polls, caches and computes attention.
- [x] Attention badges appear in the sidebar, Focus and cell headers.
- [x] Missing tokens, failed authentication and network errors are shown as described.

## M6: Native item types

See [the M6 plan](plans/m6-native-item-types.md) and [the changes after trying it](plans/m6-feedback.md).

- [x] GitLab MR (summary), MR List, CI Pipeline, CI Pipeline List, Issue (summary) and Issue List.
- [x] Jira Issue (summary) and Jira List.
- [x] CI actions: retry a pipeline or a job, cancel, play a manual job, trigger a pipeline. Other changes use the web page. See [decision 0070](decisions/0070-read-mostly-native-views.md).
- [x] Pasting a URL into the item dialog picks the right type and fills its fields. See [decision 0049](decisions/0049-items-from-a-pasted-link.md).
- [x] Tests cover URL recognition and the attention rules.

## M7: Quick add, folders and notes

See [the M7 plan](plans/m7-quick-add-and-local-items.md) and [the changes after trying it](plans/m7-feedback.md).

- [x] Quick add from a Jira key or merge request URL, through the modules' quick add handlers and related-item finders, including the project match and related merge requests.
- [x] Claude session (local) starts `claude --resume` in a terminal, on all three platforms. Dropped by [decision 0084](decisions/0084-no-local-claude-sessions.md).
- [x] The Folder / terminal item shows the branch and status, and opens a terminal, editor and file manager.
- [x] The CI Pipeline item can follow the current branch of the main folder.
- [x] Notes item with Markdown editing and preview.
- [x] Image item: the image scaled to fit, the memo under it, a click opens the link, and pasting replaces the image.

## M8: Tray, notifications and keyboard

See [the M8 plan](plans/m8-tray-notifications-and-keyboard.md). Checked by hand with [the M8 checklist](plans/m8-checks.md).

- [x] Hiding to the tray on close. The fallback works where no tray exists. ([decision 0075](decisions/0075-hide-to-tray-and-show-hide-hotkey.md))
- [x] Items can be watched. Only watched items count on the tray and send notifications. See [decision 0087](decisions/0087-watched-items.md).
- [x] The attention count on the tray icon.
- [x] Desktop notification when a linked pipeline fails. Clicking it opens the task.
- [x] The global hotkey shows or hides the app on Windows, macOS and X11, and reports clearly on Wayland.
- [x] All shortcuts in [ui](ui.md#keyboard) work, including inside focused web pages. An item can give keys to its page. See [decision 0088](decisions/0088-keys-a-page-keeps.md).
- [x] The File menu puts creating first and holds the workspace commands in a submenu.

## M9: Built-in panels

See [the M9 plan](plans/m9-built-in-panels.md).

- [ ] The `dash://` scheme, served only in the app profile, with the security headers.
- [ ] `DashBridge` provides the members listed in [web views](web-views.md#built-in-panels).
- [ ] The Task summary and Group overview panels.

## M10: Packaging

- [ ] CI builds and tests on macOS again.
- [ ] CI builds the Windows package and the macOS app bundle.
- [ ] Pushing a version tag `vX.Y.Z` publishes a GitHub release. A release workflow builds and tests the tag on Windows, macOS and Linux, and attaches a Windows setup, a macOS disk image and a Linux package to the release. The app version comes from the tag. The workflow can also run by hand for an existing tag. This follows favdir's `release.yml`.
- [ ] The macOS disk image is signed and notarised. The signing secrets live in a GitHub environment that admits only version tags.
- [ ] Windows package with `windeployqt`, macOS app bundle with `macdeployqt`. Both include Qt's `qtbase` translations, which the app loads from `QLibraryInfo::TranslationsPath`, and the WebP image plugin.
- [ ] A short install and first-run guide in the root `README.md`.

## Future: Custom builds

Not scheduled. It needs the packaging from M10. Anyone can build their own Mirante with the modules they choose, including modules from other people's repositories, and get its executables from CI. Modules stay compiled in, as in [decision 0013](decisions/0013-compiled-in-modules.md). Decision records and a code plan are written when this work starts.

**Direction:**

- **App repository.** A repository made from a GitHub template holds a manifest and a short workflow. The manifest pins the core and each module repository to a commit, and lists the modules to build one by one, each with its repository and folder. All selected modules from one repository share one commit.
- **Fetching.** The core reads the manifest and fetches the modules with CMake's FetchContent, because it must know them before it generates their registration. Git submodules, ExternalProject, package managers and a prebuilt core were considered and not chosen.
- **Repositories.** The core and the first-party modules stay in one repository while the module interface changes every milestone, so a change to the interface and to the modules that use it stays one commit. They can be split later, with `git subtree split`, if the modules need their own releases. Manifests then change only a URL.

**Acceptance criteria:**

- [ ] The core builds with module folders from outside its source tree, including one first-party module moved out as a test.
- [ ] The module interface has a version. Each module states the version it needs, and a mismatch stops configuration with a message that names the module.
- [ ] Module tests use only the module interface and its testing library, not `items/ItemTypeRegistry.h` or `mirante_items`.
- [ ] An app repository can build without the core's own tests, and still runs the tests of its modules.
- [ ] `DEPENDS` works across repositories: it links the other module and makes its headers available.
- [ ] A module can use further Qt components, and CI installs and deploys them.
- [ ] The module include check also covers external modules.
- [ ] The About dialog shows each module's source and commit.
- [ ] The core publishes a reusable workflow that builds, tests and packages an app repository. Signing secrets are not available while module code is configured or built.
- [ ] A scheduled job in the app repository opens a pull request when a pinned repository has newer commits. CI builds it, and merging it publishes a release. A failed update leaves the last release in place.
- [ ] The documentation tells module authors how to choose an id that will not clash, and tells people who build what a third-party module can reach: it runs in the app process and can read tokens and web logins. It also says that the licence of each module applies to the binary, and that builds are unsigned without the builder's own certificates.
