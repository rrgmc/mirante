# <img src="resources/icons/mirante.svg" alt="" width="40" height="40"> Mirante

Mirante is a personal desktop dashboard for all of one person's work. Work is organised in groups, projects and tasks. Each task collects its items on dashboards that the user can arrange freely. An item can be anything a module provides. The first modules cover Claude sessions, GitLab merge requests and pipelines, Jira issues, web pages, notes and project folders. It is built with Qt 6 for Windows, macOS and Linux.

The design is in [docs/README.md](docs/README.md).

![Mirante's main window. The sidebar lists groups, projects and tasks. A root dashboard shows a task's web page and notes above tabs from other tasks.](docs/images/root-dashboard.png)

## Installing

Each [release](https://github.com/rrgmc/mirante/releases) has three files:

- **Windows 10 and 11, 64-bit:** `mirante-<version>-win64.exe`. Run it and follow the steps. The setup is not signed, so the first time Windows may say that it protected your PC. Choose **More info**, then **Run anyway**. Mirante is then in the Start menu, and Settings > Apps uninstalls it.
- **macOS 12 or later, Apple silicon and Intel:** `mirante-<version>-macos.dmg`. Open it and drag Mirante to Applications. The app is signed and notarised, so it opens without a warning.
- **Ubuntu 24.04 or later, 64-bit:** `mirante_<version>_amd64.deb`. Install it with `sudo apt install ./mirante_<version>_amd64.deb`. Mirante is then in the applications menu, and `mirante` starts it from a terminal. The package installs into `/opt/mirante`, with its own Qt, and adds an AppArmor profile so the web pages keep Chromium's sandbox. `sudo apt remove mirante` removes it.

### First start

1. Mirante opens with the sample data, which shows what it can do. Changes to it are not saved.
2. Choose File > New Workspace… and pick an empty folder for your own data. A folder synced by Dropbox, OneDrive or iCloud Drive makes the workspace available on your other computers. Logins and tokens stay on each computer.
3. Choose File > New Group…, then add projects, tasks and their items from the sidebar's context menu. File > New Task from Link… makes a task from a Jira issue or a merge request link.
4. To see GitLab or Jira status, open the module's settings in the Profiles menu, such as Profiles > Default (default) > Modules > GitLab > General Configuration…, and enter the server and a token. Tokens are kept in the system's keychain.
5. Closing the window keeps Mirante in the system tray, or the menu bar on macOS. `Alt+Shift+D`, or `Control+Shift+D` on macOS, shows or hides it from any application. File > Quit ends it.

The [user interface](docs/ui.md) describes everything in detail.

## Building

[BUILDING.md](BUILDING.md) says how to build, test and package Mirante.

## Author

Rangel Reale <realerangel@gmail.com>

## License

Mirante is released under the [MIT License](LICENSE).
