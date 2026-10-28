# 0008. Claude sessions through web views and the terminal only

- Status: Accepted
- Date: 2026-10-28

## Context

Claude Remote Control and cloud sessions are listed at claude.ai/code. There is no documented public API to list them or read their state. Some community projects call the undocumented endpoints behind the web interface, using Claude Code's stored login credentials. Local Claude Code sessions can be resumed with `claude --resume <id>`.

## Decision

- Remote Control and cloud sessions are items that open their claude.ai URL in a web view.
- Local sessions are items that start `claude --resume <id>` in a terminal, in the project folder.
- The app does not use undocumented endpoints, and it does not read Claude Code's credentials.

## Alternatives

- **Calling the undocumented session endpoints,** as cc-tap does. It would allow a native session list, but can break without notice and may conflict with the terms of service.
- **Running sessions through the Claude Agent SDK or the headless CLI.** It gives full control, but the sessions would belong to the app, not to claude.ai or the phone app.
- **Embedding a terminal in the app.** Hard on Windows, which would need custom ConPTY work.

## Consequences

- Claude items show no status or summary.
- If a public sessions API appears, a new decision record will cover using it.
