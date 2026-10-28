# Mirante

Mirante is a personal desktop dashboard for all of one person's work. Work is organised in groups, projects and tasks. Each task collects its items on dashboards that the user can arrange freely. An item can be anything a module provides. The first modules cover Claude sessions, GitLab merge requests and pipelines, Jira issues, web pages, notes and project folders. It is built with Qt 6 for Windows, macOS and Linux.

The design is in [docs/README.md](docs/README.md).

## Building

Mirante is in early development. You need CMake 3.21 or newer, Ninja, and [uv](https://docs.astral.sh/uv/), which the setup scripts use to download Qt 6.8.

On Windows, with Visual Studio 2022 and its C++ tools installed:

```powershell
tools/build.ps1                  # downloads Qt if needed, builds and tests the dev preset
tools/build.ps1 -Preset release
```

On macOS and Linux:

```sh
QT_DIR=$(tools/setup-qt.sh)
cmake --preset dev -DCMAKE_PREFIX_PATH="$QT_DIR"
cmake --build --preset dev
ctest --preset dev
```

Qt goes into `.qt/` in the repository. To share one Qt between projects, set the environment variable `MIRANTE_QT_ROOT` to a folder of your choice first.

## Author

Rangel Reale <realerangel@gmail.com>

## License

Mirante is released under the [MIT License](LICENSE).
