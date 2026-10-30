# 0088. App shortcuts work in web pages, and an item can give keys to its page

- Status: Accepted
- Date: 2026-10-30

## Context

[ui](../ui.md#keyboard) says that app shortcuts work while a web page has the keyboard focus, and that shortcuts a page needs can be released per item. Web apps use many of the same keys: claude.ai, GitLab and Jira all use `Ctrl+K`, for example.

Qt WebEngine handles this in two steps. Before a key reaches the page, Qt sends a `ShortcutOverride` event to the view. WebEngine accepts it only for editing keys, such as copy, paste, undo and select all, and those keys go to the page. Every other key that matches an app shortcut runs the shortcut, and the page never sees it.

## Decision

- **App shortcuts come first.** A key that matches an app shortcut runs it, also inside a web page. Editing keys stay with the page, as WebEngine decides. So `Ctrl+V` pastes into the page, and never replaces an image item's image from there.
- **An item can give keys to its page.** A web item has a list of key sequences, stored in the item as `pageKeys` and synced. While a view of that item has the focus, those keys go to the page and no app shortcut runs.
- The item dialog offers the list for types whose views are web views, as it offers the profile. Each entry is recorded with a key sequence field.
- The global hotkey is not affected. The system handles it before any window sees it.

## Alternatives

- **Pages come first.** Every web app would keep `Ctrl+K`, `Ctrl+W` and others, and the app's keys would stop working whenever a page has the focus. That is most of the time on a dashboard of web pages.
- **One list for the whole app, or per site.** A list per item lets the same site keep a key in one cell and not in another. One list per site would need a place to keep sites, which the app does not have.
- **A key that sends the next key to the page.** Like `Ctrl+A` in `screen`. It needs no setting, but is hard to discover and slow for keys used often.

## Consequences

- `Item` gains `pageKeys`. Old versions keep it as an unknown key.
- The web views install an event filter on the view's focus proxy, which accepts `ShortcutOverride` for the item's keys.
- Keys given to a page cannot reach their app shortcut from that page. The user can still use the menus, or move the focus elsewhere first.
