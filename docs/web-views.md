# Web views

Web content uses Qt WebEngine, which embeds Chromium. This document describes login profiles, how views are created and removed, and the built-in HTML panels.

## Profiles

A `QWebEngineProfile` holds cookies, local storage, cache and site permissions. Pages in different profiles cannot see each other's logins.

The workspace holds a list of web profiles, each with an id and a name, such as "Work" and "Personal". The user manages the list with File > Web Profiles…; see [ui](ui.md#web-profiles). A profile called Default always exists and cannot be renamed or deleted. See [decision 0042](decisions/0042-user-managed-web-profiles.md).

Mirante creates these `QWebEngineProfile`s:

| Profile | Storage name | Used for |
|---|---|---|
| One per web profile | `p-<id>`, `p-default` for Default | The web items that use that profile |
| App | `app` | [Built-in panels](#built-in-panels) only |

**Which profile an item uses.** Each group has a default profile, set in the [group dialog](ui.md#group-dialog). Each item of a web type can choose its own in its properties. An item uses the first of these that exists:

1. the item's own profile;
2. its group's default profile;
3. Default.

A step that names a profile not in the list is skipped. This happens when the profile was deleted, or when its entry has not arrived from another device yet. The code is `webprofiles::effective` in `src/core/WebProfiles.h`.

**Every item in a profile can read all its logins.** Browsers keep each site's storage apart from other sites. Module code is different. A module that takes the profile with `ItemContext::profile()` can read the cookies of every site in it, and load any page in it. A Jira item and a GitLab item in the same profile can therefore reach each other's sessions. Putting items in one profile means the user trusts them together. The web profiles dialog shows this warning. To keep two things apart, use two profiles.

Rules:

- **Persistence.** All profiles are persistent, so logins survive restarts. Each one uses `ForcePersistentCookies`, because some sites set session cookies that would otherwise be lost. Site permissions are also kept on disk.
- **User agent.** Every profile sends Chromium's user agent without the `QtWebEngine/<version>` part, so sites see an ordinary Chromium. Some identity providers refuse a browser that names itself embedded. This is the first thing to adjust if a login fails.
- **Downloads.** A download asks where to save the file, starting in the system download folder.
- **Ownership.** `ProfileManager` owns every profile and creates it when first needed. A profile must outlive all pages that use it, so the manager deletes pages first at shutdown.
- **A change of profile.** When an item's profile changes, through the item, its group, or the list, its web views are made again in the new profile.
- **Deleting a profile.** Deleting a profile in the web profiles dialog deletes its logins on this device: cookies and HTTP cache are cleared, the profile is released, and its data folder is removed once the profile is gone. Chromium may hold a file open for a moment after a profile goes, so removal is tried again for a few seconds. Groups and items that named the profile are not changed; they fall back by the order above. A profile that disappears through a sync keeps its folder. Deleting a group deletes no logins. See [decision 0039](decisions/0039-profiles-deleted-only-by-local-delete.md).
- **Clearing a profile.** "Clear Data…" in the web profiles dialog deletes a profile's logins and site data on this device and keeps the profile. Qt WebEngine cannot clear the site storage of a loaded profile, so clearing works as deleting does: `ProfileManager::clearProfile()` first emits `profileClearing`, so every placement in that profile drops its view and shows a short message; then the profile is released and its folder removed; then it emits `profileCleared`, and the placements make their views again in a new, empty profile with the same name and folder. While the clearing runs, `profile()` gives a temporary profile that keeps nothing, so a page made meanwhile cannot write into the folder being removed.
- **Location.** Profile data lives in the app's local data folder, under `profiles/<storage name>`, with the HTTP cache in its `cache` subfolder. It is never synced, so the user logs in once per profile on each device.
- **Logins from before 0042.** The folders of the earlier profiles, `g-<groupId>-<moduleId>` and `shared-<moduleId>`, are deleted at start. The user logs in again once per profile.

Profiles isolate storage only. All pages run in the same Chromium processes, so this is account separation, not a security sandbox.

## Placements and live copies

A placement is one place where an item is shown: a cell in a dashboard, or the preview slot.

- Each visible placement has its own web view. If the same item is visible in two places, there are two independent views of it. They share the profile, so they share the login, but each has its own scroll position, history and unsent input. See [decision 0003](decisions/0003-view-per-placement.md).
- A view is created when its placement first becomes visible. A hidden tab does not load its pages until it is opened.
- In a dashboard, the `Placement` is the body of the item's cell. A web view's toolbar goes in the cell header, between the item name and the menu button.
- When the item's type or configuration changes, the placement makes its view again. A hidden placement does this when it is next shown. A change of name changes only the header.
- When the item or its task is removed, the placement drops its view and shows "Removed".

## Memory policy

A heavy page such as claude.ai uses a few hundred MB.

- A view whose placement becomes hidden stays loaded, so switching back is instant.
- When more than a configured number of hidden views exist (8 by default), the views hidden longest are unloaded. Their placements show a "Reload" placeholder until they become visible again, and then load automatically. The limit is the `webViews/hiddenLimit` value of the device settings.
- Only types whose `rendering()` is `Web` count. Each placement of such a type joins the `ViewPool` when it makes its view, and reports to the pool when it is shown or hidden. An unloaded placement leaves the pool until it loads again, and a deleted one leaves it at once.
- Native items, such as summaries and lists, are cheap and are never unloaded.

## Page behaviour

- **Popups.** The page handles `newWindowRequested`. A window that a page opens with a size or other features, as OAuth sign-in does, is a login popup. It opens in a dialog that uses the same profile. The dialog can be resized. It takes the size the page asks for, but at least 500 × 600 pixels, and no more than the screen. It closes when the page closes it. Every other new window or tab opens in the external browser, and no page is made for it. If such a window has no address yet, a hidden page waits for its first address, sends it to the external browser, and goes away.
- **Permissions.** Clipboard access and notifications are asked once per site and per profile. A slim bar above the page asks, for example "example.com wants to show notifications.", with Allow and Deny. Qt keeps the answer in the profile, so the site is not asked again. Camera, microphone, screen capture, location, mouse lock and local fonts are denied without asking.
- **Downloads.** Downloads go to the system download folder, after asking.
- **Toolbar.** Each web cell header has back, reload, the current address, and open in external browser. Back is disabled when there is no history. The address is cut in the middle to fit. A click on it shows the whole address, selected, so it can be copied.
- **Load failure.** When a page fails to load, a message with the error and a Reload button takes the place of the page. Chromium's own error page is turned off. A server's error page, such as a 404 page, is shown as it is: only network errors, which Chromium gives negative codes, show the message. Every failed load is logged with its error domain and code.
- **A page that stops.** If a page's process ends, for example because it crashed, the page is replaced by "The page stopped" and a Reload button, and the end is logged. Debug builds use Qt WebEngine's debug libraries, whose extra internal checks end some pages, such as the claude.ai login page, that the release libraries show. Check web pages with a release build.

## Known risk: logins in embedded browsers

Some identity providers block sign-in from embedded browsers, and Google is the best known. If claude.ai, GitLab or Jira is used through such a provider, logging in inside Mirante may fail. Workarounds:

- log in with email and password, or an email code, instead;
- open that item in the external browser instead.

[Milestone M4](roadmap.md#m4-web-views) tests this early, before more is built on web views.

Results on Windows 11, Qt WebEngine 6.8.3, release build:

| Site | How | Result |
|---|---|---|
| claude.ai | Google sign-in, with a passkey on a phone through the Windows passkey dialog and its QR code | Works, and the login and the open chat survive a restart (2026-10-29) |
| gitlab.com | | Not checked yet |
| Jira Cloud | | Not checked yet |

The debug build cannot be used for these checks: Qt WebEngine's debug libraries stop the claude.ai login page on an internal check. See [page behaviour](#page-behaviour).

## Built-in panels

Built-in panels are HTML pages shipped with the app. They show data from the [status cache](integrations.md#polling-and-caching).

- **The `dash://` scheme.** It is registered with `QWebEngineUrlScheme` before `QApplication` is created. It is served by `DashSchemeHandler` from Qt resources, at paths such as `dash://panels/task-summary/index.html`.
- **The `app` profile.** Built-in panels only load in this profile, never in a web profile of the list.
- **Content Security Policy.** Every response sends `default-src dash:; connect-src 'none'`, so panel pages cannot load remote code or call the network.
- **The bridge.** Panels talk to the app through `QWebChannel`, using one object named `dash`:

| Member | Description |
|---|---|
| `currentTask()` | The task the panel belongs to, with its items |
| `itemStatus(itemId)` | Cached status of one item |
| `groupTasks(groupId)` | Active tasks of a group, with priority and attention |
| `openItem(itemId)` | Opens an item in the app |
| `changed` (signal) | Emitted when relevant cached data changes |

The bridge never exposes tokens, connection details or raw API calls.

There are two panels in the first version: task summary and group overview. They are described in [item types](item-types.md#built-in-panels).
