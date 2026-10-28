# 0016. Generic items: any `QWidget`, with two view flags

- Status: Accepted
- Date: 2026-10-28

## Context

The documents describe GitLab, Jira, Claude and web items in detail, which makes Mirante read like a dashboard for those services. The intent is wider: a dashboard of generic items, where modules can add almost anything the platform can display.

Most such content fits in a `QWidget`. A few kinds of content break assumptions the core makes about views:

- [Decision 0003](0003-view-per-placement.md) creates one view per placement. Some content can exist only once, such as a device camera.
- `LayoutView` moves views between parent widgets when a layout is edited, so that they keep their state. Containers around native windows sometimes do not survive that move.

## Decision

- An item type's view is any `QWidget`. The core places it and never looks inside it.
- Item types can set two flags:
  - `singleView`, off by default: only one placement shows the view, and the others show a placeholder. This is an exception to decision 0003 for the types that set it.
  - `reparentSafe`, on by default: when off, the view is created again after a layout edit instead of being moved.
- The documents describe the first modules as examples of items, not as the purpose of the app.

## Alternatives

- **Generic item types configured without code**, such as a QML file, a local web app, an embedded external program or a native library loaded at run time. They add a lot of design for no current need. A QML file type remains a possible future addition.
- **No flags.** Every view would need to be creatable many times and movable. A type that cannot do both would need changes to `LayoutView` later.

## Consequences

- `LayoutView` checks the two flags from M2, although no type sets them yet.
- Embedding windows of other programs is not supported. It is fragile on Windows and impossible on macOS and Wayland.
