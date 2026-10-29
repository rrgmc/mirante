# 0037. The core makes web views; modules may take the profile instead

- Status: Accepted
- Date: 2026-10-29

## Context

The [item context](../modules.md#item-context) gave web item types a `profile()`, so each module would make its own `QWebEngineView`. Every module with a web type would then link Qt WebEngine, and repeat the popup, permission, download and toolbar handling that [web views](../web-views.md#page-behaviour) asks for. The memory policy also needs to know which views are web views.

## Decision

- `ItemContext::createWebView(url, parent)` returns a plain `QWidget` holding a core `WebPanel`, in the profile that the item's group uses for that host. This is the normal way for a web type to show a page.
- `ItemContext::profile(url)` stays, for a module that needs full control of its page. Such a module links Qt WebEngine itself, through the new `QT_LIBS` argument of `mirante_add_module()`, and handles popups, permissions and downloads on its own.
- `ItemType::rendering()` says whether a type's views are web views, which the memory policy unloads. It applies to both kinds of web view.

## Alternatives

- **Only `profile()`, as first planned.** Modules get full control of the page, but each one repeats the page behaviour, and all of them depend on Qt WebEngine.
- **Only `createWebView()`.** Page behaviour stays in one place, but a module that needs more than the core offers has to wait for a new `ItemContext` member.
- **A URL method instead of `createView` for web types.** The core would make the view from `ItemType::url()`. It is simpler for modules, but a type could not show a message, such as a missing connection, or put anything around the page.

## Consequences

- Page behaviour lives in one place, `WebPanel`, for every type that uses `createWebView()`. All five web types of M4 do.
- `profile()` returns a `QWebEngineProfile *` declared but not defined in `ItemContext.h`, so the module interface does not depend on Qt WebEngine. Only a module that calls it links Qt WebEngine.
- Tests of modules that use only `createWebView()` run without Qt WebEngine, with a fake `ItemContext`.
