# Mirante

Mirante is a personal desktop dashboard for all of one person's work. Work is organised in groups, projects and tasks. Each task collects its items on dashboards that the user can arrange freely. An item can be anything a module provides. The first modules cover Claude sessions, GitLab merge requests and pipelines, Jira issues, web pages, notes and project folders. It is built with Qt 6 for Windows, macOS and Linux.

The design is in [docs/README.md](docs/README.md).

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

On macOS, set `MIRANTE_MACOS_SIGN_IDENTITY` to the name of a code signing certificate before the first `cmake --preset`, so that macOS stops asking for keychain access after each build. `security find-identity -v -p codesigning` lists your certificates. See [decision 0087](docs/decisions/0087-sign-the-macos-bundle.md).

Each build also makes a `bin` folder in its build folder, for example `build/release/bin`. It holds a copy of the app with the Qt libraries, plugins and Qt WebEngine files it needs, so it starts without Qt on `PATH`. On macOS the folder holds the app bundle. On Linux it holds only the executable, which finds Qt on the machine that built it.

If [ccache](https://ccache.dev) is installed, the build uses it, so worktrees and presets share what they have compiled. Each still has its own build folder. To share between worktrees at different paths, tell it where they live once, with the parent folder of all of them:

```sh
ccache --set-config base_dir=<parent folder>
```

## Author

Rangel Reale <realerangel@gmail.com>

## License

Mirante is released under the [MIT License](LICENSE).
