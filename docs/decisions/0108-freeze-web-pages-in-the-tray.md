# 0108. Web pages freeze while the app is hidden to the tray

- Status: Accepted
- Date: 2026-10-31

## Context

Mirante can spend most of the day hidden in the tray ([decision 0075](0075-hide-to-tray-and-show-hide-hotkey.md)). Its web pages, such as claude.ai and Jira, keep running scripts and network requests while hidden. Chromium stops drawing a hidden page and slows its timers, but the page still uses processor time, battery and network. The user wants Mirante to use few resources while it is in the tray.

The polling of item status and the desktop notifications do not depend on the pages. They come from the native API clients in the `StatusCache`.

## Decision

- When the app hides to the tray, `PageFreezer` sets the pages of every hidden window to Qt WebEngine's `Frozen` lifecycle state. A frozen page runs no scripts and loads nothing.
- A page is frozen only when its `recommendedState` is no longer `Active`. A page that plays sound or has its developer tools open stays active, and freezes when Chromium recommends it later.
- Pages are never `Discarded`. A discarded page loads again when shown, and loses unsaved form input and its scroll position.
- The pages become active again before the windows show.
- A cell that shows the age of old data stops its timer while hidden.
- Polling of item status and notifications continue, so the tray's attention count stays current.

## Alternatives

- **Discard the pages.** This frees the most memory, but each page loads again on show and may lose what the user typed.
- **Unload the views, as the memory policy does for views hidden longest.** The same loss as discarding, and slower to come back.
- **Freeze every hidden page, also while the app is visible.** A page in a hidden tab would stop updating, such as a chat page's unread count. The user asked only about the tray.
- **Stop polling while hidden.** The tray's count and the notifications would stop, and those are what the tray is for.

## Consequences

- A page shows no new content while Mirante is hidden. It catches up after it shows, as a browser tab does after it was frozen.
- Memory use stays the same, since the pages stay loaded. The [memory policy](../web-views.md#memory-policy) still limits how many hidden views stay loaded.
- Minimizing the window without a tray does not freeze the pages.
