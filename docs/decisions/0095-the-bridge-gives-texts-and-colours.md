# 0095. The bridge gives panels their texts and colours

- Status: Accepted
- Date: 2026-10-30

## Context

Built-in panels are HTML pages. The rest of the app is translated with Qt's tools into Brazilian Portuguese ([decision 0086](0086-english-and-brazilian-portuguese.md)), formats dates and numbers for its language, and follows the system's light or dark palette. A page knows none of this by itself. Qt WebEngine's `prefers-color-scheme` does not reliably follow the app's palette, and texts written in JavaScript would not reach Qt Linguist.

## Decision

- **Texts from the bridge.** `dash.texts()` returns every fixed text a panel shows, such as "No items report a status", translated with `tr()` in the bridge. Panels hold no text of their own beyond the keys.
- **Formatted values from the bridge.** Values that depend on the language come formatted, such as `"updated": "5 minutes ago"` and the name of a priority. Panels do no date or number formatting.
- **Colours from the bridge.** `dash.palette()` returns the app's colours as CSS colours: window, base, text, muted text, accent, link, and the colours of priorities and attention badges that the sidebar already uses. Panels set them as CSS custom properties. The `changed` signal also fires when the palette changes.
- **The bridge members.** The table in [web views](../web-views.md#built-in-panels) gains `openTask(taskId)`, `texts()` and `palette()`. Group overview needs `openTask` to open a task from its list.

## Alternatives

- **Texts in JavaScript, with a table per language.** It keeps the panels self-contained, but adds a second translation format next to Qt's, which `lupdate` and the language setting know nothing about.
- **`prefers-color-scheme` in CSS.** Simple, but follows Chromium's idea of the system theme, not the app's palette, and gives no access to the group and priority colours.

## Consequences

- A new panel needs its texts added to the bridge, so a panel change can be a C++ change too.
- The panels look like the rest of the app in light and dark, and switch when the system does.
- Tests can check the texts and colours as plain JSON, without a page.
