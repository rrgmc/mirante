# Web views

Web content uses Qt WebEngine, which embeds Chromium. This document describes login profiles, how views are created and removed, and the built-in HTML panels.

## Profiles

A `QWebEngineProfile` holds cookies, local storage, cache and site permissions. Pages in different profiles cannot see each other's logins.

Mirante creates these profiles:

| Profile | Storage name | Used for |
|---|---|---|
| One per group and module | `g-<groupId>-<moduleId>` | All web items of that module in that group, by default |
| Shared, one per module | `shared-<moduleId>` | Sites that a group sends to the shared profile |
| App | `app` | [Built-in panels](#built-in-panels) only |

The module of an item is the prefix of its type id, before the first dot, such as `gitlab` for `gitlab.mr.page`. Each module has its own profiles, so module code cannot read the logins of another module. The cost is that the same site is logged in to once per module and group. See [decision 0040](decisions/0040-profiles-per-group-and-module.md).

Rules:

- **Persistence.** All profiles are persistent, so logins survive restarts. Each one uses `ForcePersistentCookies`, because some sites set session cookies that would otherwise be lost. Site permissions are also kept on disk.
- **Overrides per group.** A group's `profileOverrides` can send a site host to the shared profile of the item's module. Hosts match exactly, ignoring case, so `jira.example.com` does not cover `a.jira.example.com`. For example, the user can log in to Jira once in the shared profile, while claude.ai stays separate for each group. See [decision 0005](decisions/0005-profiles-per-group.md).
- **User agent.** Every profile sends Chromium's user agent without the `QtWebEngine/<version>` part, so sites see an ordinary Chromium. Some identity providers refuse a browser that names itself embedded. This is the first thing to adjust if a login fails.
- **Downloads.** A download asks where to save the file, starting in the system download folder.
- **Ownership.** `ProfileManager` owns every profile and creates it when first needed. A profile must outlive all pages that use it, so the manager deletes pages first at shutdown.
- **Deleting a group.** This deletes the profiles of all its modules: cookies and HTTP cache are cleared, the profiles are released, and their data folders are removed once the profiles are gone. The folders are found by their `g-<groupId>-` prefix, so the folders of modules missing from this build go too. Chromium may hold a file open for a moment after a profile goes, so removal is tried again for a few seconds. Only a group deleted by the user on this device loses its profiles, not one removed by a sync. See [decision 0039](decisions/0039-profiles-deleted-only-by-local-delete.md).
- **Location.** Profile data lives in the app's local data folder, under `profiles/<storage name>`, with the HTTP cache in its `cache` subfolder. It is never synced.

Profiles isolate storage only. All pages run in the same Chromium processes, so this is account separation, not a security sandbox.

## Placements and live copies

A placement is one place where an item is shown: a cell in a dashboard, or an item tab.

- Each visible placement has its own web view. If the same item is visible in two places, there are two independent views of it. They share the profile, so they share the login, but each has its own scroll position, history and unsent input. See [decision 0003](decisions/0003-view-per-placement.md).
- A view is created when its placement first becomes visible. A hidden tab does not load its pages until it is opened.

## Memory policy

A heavy page such as claude.ai uses a few hundred MB.

- A view whose placement becomes hidden stays loaded, so switching back is instant.
- When more than a configured number of hidden views exist (8 by default), the views hidden longest are unloaded. Their placements show a "Reload" placeholder until they become visible again, and then load automatically.
- Native items, such as summaries and lists, are cheap and are never unloaded.

## Page behaviour

- **Popups.** `WebPanel` overrides `createWindow`. Login popups, for example OAuth sign-in, open in a small dialog using the same profile and close when the page closes them. Ordinary "open in new window" links open in the external browser.
- **Permissions.** Clipboard access and notifications are asked once per host and per profile, and the answer is remembered. Camera, microphone and location are always denied.
- **Downloads.** Downloads go to the system download folder, after asking.
- **Toolbar.** Each web cell header has back, reload, the current address, and open in external browser.

## Known risk: logins in embedded browsers

Some identity providers block sign-in from embedded browsers, and Google is the best known. If claude.ai, GitLab or Jira is used through such a provider, logging in inside Mirante may fail. Workarounds:

- log in with email and password, or an email code, instead;
- open that item in the external browser instead.

[Milestone M4](roadmap.md#m4-web-views) tests this early, before more is built on web views.

## Built-in panels

Built-in panels are HTML pages shipped with the app. They show data from the [status cache](integrations.md#polling-and-caching).

- **The `dash://` scheme.** It is registered with `QWebEngineUrlScheme` before `QApplication` is created. It is served by `DashSchemeHandler` from Qt resources, at paths such as `dash://panels/task-summary/index.html`.
- **The `app` profile.** Built-in panels only load in this profile, never in a group profile.
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
