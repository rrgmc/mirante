# Web views

Web content uses Qt WebEngine, which embeds Chromium. This document describes login profiles, how views are created and removed, and the built-in HTML panels.

## Profiles

A `QWebEngineProfile` holds cookies, local storage, cache and site permissions. Pages in different profiles cannot see each other's logins.

The workspace holds a list of [profiles](data-model.md#profile), each with an id and a name, such as "Work" and "Personal". A profile holds web logins and module settings, with the server and token of each module; this section is about its web logins. The user manages the list with Profiles > Manage Profiles…; see [ui](ui.md#profiles). There is always at least one profile, and one of them is the default. Any profile can be renamed, and any but the last can be deleted. See decisions [0042](decisions/0042-user-managed-web-profiles.md), [0050](decisions/0050-profiles-hold-logins-connections-and-settings.md) and [0057](decisions/0057-any-profile-can-be-the-default.md).

Mirante creates these `QWebEngineProfile`s:

| Profile | Storage name | Used for |
|---|---|---|
| One per profile of the open workspace | `<workspace id>_p-<profile id>`, such as `…_p-default` for the profile with the id `default` | The web items that use that profile |
| App | `app` | [Built-in panels](#built-in-panels) only |

**Which profile an item uses.** Each group can choose a profile, in the [group dialog](ui.md#group-dialog). Each item of a type that uses a profile can choose its own in its properties. An item uses the first of these that exists:

1. the item's own profile;
2. its group's profile;
3. the workspace's default profile.

A step that names a profile not in the list is skipped. This happens when the profile was deleted, or when its entry has not arrived from another device yet. The code is `profiles::effective` in `src/core/Profiles.h`. The same profile gives the item its module settings, with its servers.

**Every item in a profile can read all its logins.** Browsers keep each site's storage apart from other sites. Module code is different. A module that takes the profile with `ItemContext::profile()` can read the cookies of every site in it, and load any page in it. A Jira item and a GitLab item in the same profile can therefore reach each other's sessions. They also share the profile's servers and tokens. Putting items in one profile means the user trusts them together. The profiles dialog shows this warning. To keep two things apart, use two profiles.

Rules:

- **Persistence.** All profiles are persistent, so logins survive restarts. Each one uses `ForcePersistentCookies`, because some sites set session cookies that would otherwise be lost. Site permissions are also kept on disk.
- **User agent.** Every profile sends Chromium's user agent without the `QtWebEngine/<version>` part, so sites see an ordinary Chromium. Some identity providers refuse a browser that names itself embedded. This is the first thing to adjust if a login fails.
- **Downloads.** A download asks where to save the file, starting in the system download folder.
- **Ownership.** `ProfileManager` owns every profile and creates it when first needed. A profile must outlive all pages that use it, so the manager deletes pages first at shutdown.
- **A change of profile.** When an item's profile changes, through the item, its group, or the list, its web views are made again in the new profile. A change of the module settings of its profile also makes them again, since a view may have read them. A new name does not.
- **Deleting a profile.** Deleting a profile in the profiles dialog deletes its logins on this device: cookies and HTTP cache are cleared, the profile is released, and its data folder is removed once the profile is gone. Chromium may hold a file open for a moment after a profile goes, so removal is tried again for a few seconds. Groups and items that named the profile are not changed; they fall back by the order above. A profile that disappears through a sync keeps its folder. Deleting a group deletes no logins. See [decision 0039](decisions/0039-profiles-deleted-only-by-local-delete.md).
- **Clearing a profile.** "Clear Data…" in the profiles dialog deletes a profile's logins and site data on this device and keeps the profile, with its settings. Qt WebEngine cannot clear the site storage of a loaded profile, so clearing works as deleting does: `ProfileManager::clearProfile()` first emits `profileClearing`, so every placement in that profile drops its view and shows a short message; then the profile is released and its folder removed; then it emits `profileCleared`, and the placements make their views again in a new, empty profile with the same name and folder. While the clearing runs, `profile()` gives a temporary profile that keeps nothing, so a page made meanwhile cannot write into the folder being removed.
- **Location.** Profile data lives in the app's local data folder, under `profiles/<workspace id>/p-<profile id>`, with the HTTP cache in its `cache` subfolder. Each workspace keeps its own logins: two workspaces with a profile of the same id do not share them. The workspace id is kept in `workspace.mirante`, so moving or renaming the workspace keeps its logins. The app profile is in `profiles/app`. Profile data is never synced, so the user logs in once per profile on each device. See [decision 0059](decisions/0059-logins-per-workspace.md).
- **One process per web data folder.** Qt WebEngine, unlike Chrome, does not stop two processes from using one profile folder, and two would damage each other's cookie and site databases. Mirante therefore locks its web data folder, the `profiles` folder that holds every workspace's logins, at start, with `.lock` in it, before the `ProfileManager` is made. A later start hands its `--data` over to the running instance and exits. `--web-data <folder>` gives a test run its own folder. See [decision 0058](decisions/0058-one-instance-per-web-data-folder.md).
- **A workspace id changed on another device.** Two devices can each give a workspace without an id its own id, and the last file written wins. Each device remembers the id it last opened a workspace folder with. When the file's id differs, the logins of the remembered id move to the new one as the workspace opens. An id this device gives itself takes nothing. The profiles' tokens move with the logins; see [sync](sync.md#secrets). See decisions [0062](decisions/0062-logins-follow-a-new-workspace-id.md) and [0063](decisions/0063-tokens-follow-a-new-workspace-id.md).
- **Logins from before 0059.** They were kept in `profiles/p-<profile id>`, for all workspaces. The first workspace from before decision 0059 that is opened on the device takes them, so the user stays logged in there.
- **The first folder stays.** Qt 6.8 makes a named profile in a folder of its own first, under its default data path (`%APPDATA%\Mirante\Mirante\QtWebEngine\<storage name>` on Windows), and only then lets `ProfileManager` move it to its real folder. Chromium keeps using that first path for some storage. On Windows, if the folder is gone, Chromium fails a check that stops the whole app as soon as a page needs it, such as the Atlassian login page. So the empty first folders are never removed. macOS has no such check.
- **Logins from before 0042.** The folders of the earlier profiles, `g-<groupId>-<moduleId>` and `shared-<moduleId>`, are deleted at start. The user logs in again once per profile.

Profiles isolate storage only. All pages run in the same Chromium processes, so this is account separation, not a security sandbox.

## Placements and live copies

A placement is one place where an item is shown: a cell in a dashboard, or the preview slot.

- Each visible placement has its own web view. If the same item is visible in two places, there are two independent views of it. They share the profile, so they share the login, but each has its own scroll position, history and unsent input. See [decision 0003](decisions/0003-view-per-placement.md).
- A view is created when its placement first becomes visible. A hidden tab does not load its pages until it is opened.
- In a dashboard, the `Placement` is the body of the item's cell. A web view's toolbar goes in the cell header, between the item name and the menu button.
- When the item's type or configuration changes, the placement makes its view again. A hidden placement does this when it is next shown. A change of name changes only the header.
- When the item or its task is removed, the placement drops its view and shows "Removed".
- A native view can show a web page in its place with `ItemContext::showPage()`. The view stays alive and hidden. The cell header shows a "Back to summary" button and the page's toolbar. Back drops the page and shows the same view again. A change of the item's configuration, or of the profile the page was made in, also drops the page, and a further `showPage()` replaces it. The page is not stored. See [decision 0069](decisions/0069-open-page-in-the-placement.md).

## Memory policy

A heavy page such as claude.ai uses a few hundred MB.

- A view whose placement becomes hidden stays loaded, so switching back is instant.
- When more than a configured number of hidden views exist (8 by default), the views hidden longest are unloaded. Their placements show a "Reload" placeholder until they become visible again, and then load automatically. The limit is the `webViews/hiddenLimit` value of the device settings.
- Only types whose `rendering()` is `Web` count, and pages of `showPage()`. Each placement of such a type joins the `ViewPool` when it makes its view, and reports to the pool when it is shown or hidden. An unloaded placement leaves the pool until it loads again, and a deleted one leaves it at once.
- A placement with a page of `showPage()` has one entry in the pool. When the pool unloads the page, the placement goes back to its view instead of showing "Reload".
- Native items, such as summaries and lists, are cheap and are never unloaded.

## Page behaviour

- **Popups.** The page handles `newWindowRequested`. A window that a page opens with a size or other features, as OAuth sign-in does, is a login popup. It opens in a dialog that uses the same profile. The dialog can be resized. It takes the size the page asks for, but at least 500 × 600 pixels, and no more than the screen. It closes when the page closes it. Every other new window or tab opens in the external browser, and no page is made for it. If such a window has no address yet, a hidden page waits for its first address, sends it to the external browser, and goes away.
- **Permissions.** Clipboard access and notifications are asked once per site and per profile. A slim bar above the page asks, for example "example.com wants to show notifications.", with Allow and Deny. Qt keeps the answer in the profile, so the site is not asked again. Camera, microphone, screen capture, location, mouse lock and local fonts are denied without asking.
- **Downloads.** Downloads go to the system download folder, after asking.
- **Toolbar.** Each web cell header has back, reload, the current address, and open in external browser. Back is disabled when there is no history. The address is cut in the middle to fit. A click on it shows the whole address, selected, so it can be copied.
- **Keys.** App shortcuts come first, also while a page has the focus. Qt sends a `ShortcutOverride` event to the focused widget before it runs a shortcut, and a widget that accepts it gets the key instead. Qt WebEngine accepts it for editing keys in a page's text field, `Ctrl+B` among them. `PageKeys`, an event filter on every web view and on the focus proxy that WebEngine adds later, accepts it for the item's `pageKeys` too. See [decision 0088](decisions/0088-keys-a-page-keeps.md) and [keyboard](ui.md#keyboard).
- **Load failure.** When a page fails to load, a message with the error and a Reload button takes the place of the page. Chromium's own error page is turned off. A server's error page, such as a 404 page, is shown as it is: only network errors, which Chromium gives negative codes, show the message. Every failed load is logged with its error domain and code.
- **A page that stops.** If a page's process ends, for example because it crashed, the page is replaced by "The page stopped" and a Reload button, and the end is logged. In a debug build this happens to some sites that work in a release build; see [debug builds](#debug-builds).

## Debug builds

A debug build links Qt WebEngine's debug libraries, such as `Qt6WebEngineCored.dll` on Windows. They run Chromium's internal checks, which the release libraries leave out. When a check fails, Chromium stops the process it runs in on purpose. Windows reports this as an app crash with the exception code `0x80000003`, a breakpoint, in the WebEngine library.

**A page that stops.** Some real sites fail a check in Blink's style code, about CSS animations, in a page's render process. On Windows, with Qt 6.8.3, this happened on 2026-10-29 to the claude.ai pages and to the Atlassian login page, `id.atlassian.com`. Only that page's process ends: the cell shows "The page … stopped" with a Reload button, and the rest of the app keeps running. Reload fails the same way. The same pages load and sign in with a release build. This is not a Mirante bug, and nothing in Mirante can avoid it.

**Tests on the offscreen platform.** A page loaded while its window is hidden, and shown later, fails a check about overlays (`!m_scopedOverlayReadAccess`) as soon as it is used, so tests show the window first, as the app does. Paste and copy into a page also fail it, as do some keys after many app shortcuts, so the key test of `tst_page_keys` runs only with release libraries, as in CI.

**The whole app stops.** A failed check in Chromium's main process ends the app. That is worth investigating, because the cause can be in Mirante: the release libraries skip the check, but the same mistake may fail later in a way that is harder to see. An example is the first folder of a profile; see the rules under [profiles](#profiles).

**Finding the failed check.** Chromium writes the reason to standard error just before it stops, in a line with `FATAL` and `Check failed`, followed by a stack trace. Mirante is a window app, so on Windows that output goes nowhere unless it is captured. Start the app with standard error sent to a file, for example in PowerShell:

```powershell
$qt = & tools/setup-qt.ps1 | Select-Object -Last 1
$env:PATH = "$qt\bin;$env:PATH"
Start-Process build\dev\mirante.exe -RedirectStandardError err.txt `
    -ArgumentList '--settings', 'test.ini', '--web-data', 'test-web-data'
```

The separate settings file and web data folder keep the test run apart from a Mirante that is already running; without `--web-data`, the new start would hand over to it and exit.

Then look for `FATAL` in `err.txt`. Windows' own record, in the Application event log under "Application Error" and "Windows Error Reporting", only names the library and the exception code.

**Rules:**

- Check web pages, logins in particular, with a release build: `tools/run.ps1 -Preset release -Build`.
- When a page stops only in a debug build, try a release build before looking further.
- When the whole app stops, read the `FATAL` line before changing anything.

## Known risk: logins in embedded browsers

Some identity providers block sign-in from embedded browsers, and Google is the best known. If claude.ai, GitLab or Jira is used through such a provider, logging in inside Mirante may fail. Workarounds:

- log in with email and password, or an email code, instead;
- open that item in the external browser instead.

[Milestone M4](roadmap.md#m4-web-views) tests this early, before more is built on web views.

Results on Windows 11, Qt WebEngine 6.8.3, release build:

| Site | How | Result |
|---|---|---|
| claude.ai | Google sign-in, with a passkey on a phone through the Windows passkey dialog and its QR code | Works, and the login and the open chat survive a restart (2026-10-29) |
| gitlab.com | | Works (2026-10-29) |
| Jira Cloud | Atlassian login page, `id.atlassian.com` | Works (2026-10-29) |

The debug build cannot be used for these checks: Qt WebEngine's debug libraries stop the claude.ai pages and the Atlassian login page on an internal check. See [debug builds](#debug-builds).

## Built-in panels

Built-in panels are HTML pages shipped with the app. They show data from the [status cache](integrations.md#polling-and-caching).

- **The `dash://` scheme.** It is registered with `QWebEngineUrlScheme` before `QApplication` is created, by `dash::registerScheme()`. It has host syntax and is secure and local: a panel is a secure context, and pages of other schemes cannot load it. It is served by `DashSchemeHandler` from Qt resources: `dash://panels/<path>` is the resource `:/panels/<path>`, such as `dash://panels/task-summary/index.html`. Another host, a path with an empty part, `.` or `..`, a folder, or a file other than HTML, JavaScript, CSS or SVG fails as not found. Only `GET` is answered. `dash://panels/common/qwebchannel.js` is Qt's own `qwebchannel.js`, because the policy below does not allow `qrc:`.
- **The `app` profile.** Only this profile installs the handler, in `ProfileManager::appProfile()`. A profile of the list fails a `dash:` address, so built-in panels load only in the app profile.
- **The headers.** Every response sends:

  ```text
  Content-Security-Policy: default-src dash:; connect-src 'none'; object-src 'none'; base-uri 'none'; form-action 'none'; frame-ancestors 'none'
  X-Content-Type-Options: nosniff
  Referrer-Policy: no-referrer
  Cache-Control: no-store
  ```

  Panel pages cannot load remote code, run inline scripts or call the network. `no-store` makes a panel changed in a new build load fresh.
- **The bridge.** Panels talk to the app through `QWebChannel`, using one object named `dash`:

| Member | Description |
|---|---|
| `currentTask()` | The task the panel belongs to, with its items |
| `itemStatus(itemId)` | Cached status of one item |
| `groupTasks(groupId)` | Active tasks of a group, with priority and attention |
| `openItem(itemId)` | Opens an item in the app |
| `changed` (signal) | Emitted when relevant cached data changes |

The bridge never exposes tokens, server settings or raw API calls.

There are two panels in the first version: task summary and group overview. They are described in [item types](item-types.md#built-in-panels).
