# 0084. No Claude session (local) item

- Status: Accepted
- Date: 2026-10-30

## Context

[Decision 0008](0008-claude-sessions.md) gave local Claude Code sessions an item that runs `claude --resume <id>` in a terminal. M7 built it. After trying it, the user found it not practical: the session id has to be found by hand, and a session is short-lived compared with a task.

## Decision

- The `claude` module keeps only Claude session (web). The type `claude.session.local` is no longer registered.
- The `claude` module's device setting for the Claude Code program goes, and so does the device name, which only that item used.
- An item of the removed type that is already in a workspace is kept unchanged, like an item of a missing module. Its cell says "This item type is no longer available: claude.session.local." and the user can delete it.
- The folder item's **Terminal** button stays. Starting a program in a terminal stays in `ItemContext` and `Launcher`, for modules that need it.

This supersedes the part of decision 0008 about local sessions.

## Alternatives

- **Listing local sessions from Claude Code's files.** It would remove the typing, but decision 0008 keeps the app out of Claude Code's stored data, and the files' format is not documented.
- **Keeping the type but hiding it from new items.** It keeps code that nobody uses.

## Consequences

- The M7 box for local sessions is closed by this decision instead of by the feature.
- Synced data from a device that made such items still loads and saves unchanged.
