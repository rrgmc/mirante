# 0105. Mirante can start at sign-in, hidden in the tray

- Status: Accepted
- Date: 2026-10-31

## Context

Mirante hides to the tray and comes back with a global hotkey ([decision 0075](0075-hide-to-tray-and-show-hide-hotkey.md)). That decision left starting at sign-in out. Without it, the user starts Mirante by hand after each sign-in. favdir, the user's other desktop app, starts at sign-in, and its setup offers this and a "Run" option on its last page.

## Decision

- The settings window's Window section has "Start Mirante when I sign in". The system holds this choice, not the device settings, so a change made in the system's own settings shows there.
  - Windows: a value named `Mirante` in the per-user Run key, `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`, which runs `mirante.exe --hidden`.
  - macOS 13 and later: `SMAppService.mainAppService`. macOS may ask the user to allow it in System Settings > General > Login Items; Mirante then offers to open that page. Before macOS 13 the option is not shown.
  - Linux: an XDG autostart file, `~/.config/autostart/mirante.desktop`, which runs Mirante with `--hidden`.
- Started at sign-in, Mirante keeps its window hidden in the tray until it is called by the tray, the global hotkey or a second start. The `--hidden` argument does this on Windows and Linux. A macOS login item gets no arguments, so Mirante reads the launch event instead. If no tray appears within the minute Mirante waits for one, the window shows minimized.
- The Windows setup's last page has two checkboxes: "Run Mirante" and "Start Mirante when I sign in". The second is ticked on a first install and keeps the installed version's choice on an upgrade. A silent install keeps the choice too. The uninstaller removes the Run value. The setup starts Mirante through Explorer, so it runs as the signed-in user and not elevated.
- The macOS disk image and the Linux package have no setup pages, so they offer only the settings window's option.
- The code lives in `src/platform/LoginItem.h`, after favdir's.

## Alternatives

- **A device setting that Mirante applies at each start.** The system's own list would disagree with it after a change made there.
- **Start with the window shown.** At sign-in the window would take the screen before the user asks for it.
- **A launch agent file on macOS.** `SMAppService` is the supported way from macOS 13, and it shows Mirante by name in Login Items.

## Consequences

- A test run never changes the sign-in entry: only `main()` lets the settings window offer the option.
- A development build that ticks the option points the entry at its own executable, until the option is ticked again from the installed app.
- The NSIS code depends on two lines of CPack's NSIS template; the configure step stops with a message if they change.
