# Web views

Web content uses Qt WebEngine, which embeds Chromium. This document describes login profiles, how views are created and removed, and the built-in HTML panels.

## Profiles

A `QWebEngineProfile` holds cookies, local storage, cache and site permissions. Pages in different profiles cannot see each other's logins.

Mirante creates these profiles:

| Profile | Storage name | Used for |
|---|---|---|
| One per group | `g-<groupId>` | All web items of that group, by default |
| Shared | `shared` | Sites that a group sends to the shared profile |
| App | `app` | [Built-in panels](#built-in-panels) only |

Rules:

- **Persistence.** All profiles are persistent, so logins survive restarts. Each one uses `ForcePersistentCookies`, because some sites set session cookies that would otherwise be lost.
- **Overrides per group.** A group's `profileOverrides` can send a site host to the shared profile. For example, the user can log in to Jira once in the shared profile, while claude.ai stays separate for each group. See [decision 0005](decisions/0005-profiles-per-group.md).
- **Ownership.** `ProfileManager` owns every profile and creates it when first needed. A profile must outlive all pages that use it, so the manager deletes pages first at shutdown.
- **Deleting a group.** This deletes its profile: cookies are cleared through `cookieStore()`, and the profile's data folder is removed after the profile is released.
- **Location.** Profile data lives in the app's local data folder. It is never synced.

Profiles isolate storage only. All pages run in the same Chromium processes, so this is account separation, not a security sandbox.

## Placements and live copies

A placement is one place where an item is shown: a cell in a dashboard, a tab, or a detached window.

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
- **Toolbar.** Each web cell header has back, reload, the current address, open in external browser, and detach.

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
