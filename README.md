# <img src="resources/icons/mirante.svg" alt="" width="40" height="40"> Mirante

Mirante is a personal desktop dashboard for all of one person's work. Work is organised in groups, projects and tasks. Each task collects its items on dashboards that the user can arrange freely. An item can be anything a module provides. The first modules cover Claude sessions, GitLab merge requests and pipelines, Jira issues, web pages, notes and project folders. It is built with Qt 6 for Windows, macOS and Linux.

The design is in [docs/README.md](docs/README.md).

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

Mirante is in early development. You need CMake 3.21 or newer, Ninja, Python 3, and [uv](https://docs.astral.sh/uv/), which the setup scripts use to download Qt 6.8. Besides Qt itself, they download Qt WebEngine, `qttools` for `lupdate` and `lrelease`, Qt's translations, and `qtimageformats` for WebP images. A Qt downloaded before these were added is completed on the next build.

With [Task](https://taskfile.dev) installed, the common commands are the same on every platform:

```sh
task                         # lists the tasks
task build                   # builds the dev preset
task test PRESET=release     # builds and tests another preset
task check                   # the include and translation checks, then tests with all modules and with none
task translations            # updates translations/ after a text changed; see translations/README.md
task run -- -Data <folder>   # starts the build, on Windows
```

Without it, on Windows, with Visual Studio 2022 and its C++ tools installed:

```powershell
tools/build.ps1                  # downloads Qt if needed, builds and tests the dev preset
tools/build.ps1 -Preset release
tools/run.ps1                    # starts the dev build; -Data <folder> opens that workspace, -Build builds first
tools/run.ps1 -Data <folder> -Settings <file.ini>   # a test run that leaves this device's settings alone
```

On macOS and Linux:

```sh
QT_DIR=$(tools/setup-qt.sh)
cmake --preset dev -DCMAKE_PREFIX_PATH="$QT_DIR"
cmake --build --preset dev
ctest --preset dev
```

Qt goes into `.qt/` in the repository. To share one Qt between projects, set the environment variable `MIRANTE_QT_ROOT` to a folder of your choice first.

On macOS, the build signs the app with a code signing certificate, so that macOS stops asking for keychain access after each build. It uses the maintainer's development certificate, named in `CMakeLists.txt`, when the keychain has it. Otherwise, set `MIRANTE_MACOS_SIGN_IDENTITY` to the name of one of your certificates before the first `cmake --preset`. `security find-identity -v -p codesigning` lists them. Without a certificate the app is signed ad hoc, and macOS asks again after each build. See decisions [0090](docs/decisions/0090-sign-the-macos-bundle.md) and [0091](docs/decisions/0091-default-macos-sign-identity.md).

Each build also makes a `bin` folder in its build folder, for example `build/release/bin`. It holds a copy of the app with the Qt libraries, plugins and Qt WebEngine files it needs, so it starts without Qt on `PATH`. On macOS the folder holds the app bundle. On Linux it holds only the executable, which finds Qt on the machine that built it.

To make the package of a build, install it and run CPack in its build folder. The install folder must be an absolute path. On Windows, `tools/build.ps1 -Preset release -Package` does all of it. NSIS must be installed.

```sh
cmake --install build/release --component Mirante --prefix "$PWD/build/release/install"
python3 tools/check-package.py build/release/install
cd build/release && cpack
```

[docs/releasing.md](docs/releasing.md) says how to publish a release.

If [ccache](https://ccache.dev) is installed, the build uses it, so worktrees and presets share what they have compiled. Each still has its own build folder. To share between worktrees at different paths, tell it where they live once, with the parent folder of all of them:

```sh
ccache --set-config base_dir=<parent folder>
```

## Author

Rangel Reale <realerangel@gmail.com>

## License

Mirante is released under the [MIT License](LICENSE).
