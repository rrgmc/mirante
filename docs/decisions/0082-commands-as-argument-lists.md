# 0082. Commands are passed as a program and its arguments

- Status: Accepted
- Date: 2026-10-30

## Context

M7 starts programs: `claude --resume <id>` in a terminal, a terminal on its own, and an editor. Part of each command comes from synced data, such as a session id, or from a path on disk that may hold spaces. Each platform's terminal reads its command line differently: `wt.exe` splits on `;`, `cmd.exe` has its own quoting, and macOS goes through AppleScript and a shell.

## Decision

- Modules pass a program and a list of arguments to `ItemContext::launchTerminal()` and `startProgram()`, never one command string.
- The core's `Launcher` builds the platform's command from them, quoting each argument for every layer it passes through.
- The terminal command device setting is a template with `{dir}` and `{command}`. `{command}` becomes the program and its arguments as separate arguments.
- On Windows, a program that is a `.cmd` or `.bat` file, such as `code`, runs through `cmd.exe /c`.
- Values from synced data are also checked where they are read. A Claude session id holds letters, digits and dashes only.

## Alternatives

- **One command string, with placeholders.** Simple to configure, but every module would have to quote for every platform, and a mistake runs a different command.
- **Start programs without a terminal.** Claude Code needs a terminal to run in.

## Consequences

- The command building is a pure function per platform, so the macOS and Linux quoting is tested on Windows.
- A terminal template that needs one string, such as `sh -c`, must put `{command}` where a list of arguments fits.
