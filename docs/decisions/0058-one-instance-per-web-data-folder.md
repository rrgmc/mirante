# 0058. One instance per web data folder

- Status: Accepted
- Date: 2026-10-29

## Context

The profiles' web logins live in the web data folder: the app's local data folder plus `/profiles`. See [web views](../web-views.md#profiles). Chrome stops two processes from using one profile folder. Qt WebEngine does not. Two Mirante processes would write the same cookie and site databases at once, and could lose logins or damage the databases.

Test runs still need a second Mirante beside the user's own, with its own logins.

## Decision

- **One process per web data folder.** At start, before the `ProfileManager` exists, Mirante takes a lock on the file `.lock` in its web data folder, with `QLockFile`. The lock is held until the app quits.
- **A crashed holder's lock is taken over.** The lock has no age limit, because the app holds it for hours. A lock whose process no longer runs on this computer is stale, and `QLockFile` removes it and takes the lock.
- **A later start hands over.** When the lock is held, the new process connects to the holder through a local socket. The socket's name is `mirante-` and a hash of the folder's absolute path and the user's home folder. The new process sends one line of JSON, with the absolute path given with `--data` if any, such as `{"data":"C:/work"}`. The holder answers `ok`, opens that workspace, and brings its main window to the front. The new process then exits with code 0.
- **No answer.** The new process tries for five seconds, since a holder that has just started may not listen yet. If no answer comes, it tries the lock again, in case the holder quit meanwhile. Otherwise it says that Mirante is already running with this web data folder, and exits with code 1.
- **Windows foreground.** Windows does not let a background process take the foreground. The new process, which the user just started, calls `AllowSetForegroundWindow` for the holder's process id before it sends. The code is in `src/platform/Foreground.cpp`; on macOS and Linux it does nothing.
- **`--web-data <folder>`** uses another web data folder, so a test run can run beside the user's own instance. `tools/run.ps1` has `-WebData <folder>`. `--settings` does not change the web data folder.

The code is `SingleInstance` in `src/web`, which links Qt Network for the local socket. `main.cpp` wires its `activationRequested` signal to the main window.

## Alternatives

- **One instance per workspace.** Two workspaces could then be open at once, but they would still share the web data folder, which is the thing to protect.
- **One instance for the whole app, with no option.** Simpler, but test runs could not run beside the user's own instance.
- **Only a lock, with no handover.** A second start would show an error even when the user only wanted the window or another workspace.
- **A lock file without `QLockFile`.** `QLockFile` already handles stale locks, the race of two starts, and two processes removing one stale lock.

## Consequences

- Starting Mirante a second time brings the running window forward, and opens the workspace given with `--data` in it.
- Two starts at the same moment are safe: only one gets the lock, and the other waits for it to listen.
- A holder that hangs blocks later starts until it is closed.
- This complements [decision 0039](0039-profiles-deleted-only-by-local-delete.md) and the [profiles](../web-views.md#profiles) section of web views: the web data folder is never synced, and now also never shared between two processes.
