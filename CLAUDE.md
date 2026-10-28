# rraidash

A personal "all work" desktop dashboard in Qt 6 (C++17, Qt Widgets, CMake) for Windows, macOS and Linux. Start with [docs/README.md](docs/README.md).

## Git workflow

- Work directly on `master` while the project is in rapid initial development. Do not create feature branches. This overrides the global branching rule.
- Commit as work progresses, one focused commit per finished piece.
- Push to `master` only when the user asks.

## Documentation

- The design lives in `docs/`. Read the relevant documents before changing behaviour, and update them in the same commit when the behaviour changes.
- Record each significant design decision as a new file in `docs/decisions/`, using the template in `docs/decisions/README.md`. Do not rewrite an accepted decision; add a new one that supersedes it.
- Write a detailed code plan for a milestone just before implementing it, not earlier. The milestones are in `docs/roadmap.md`.
- Use the terms defined in the glossary in `docs/README.md`.

## Building

- Qt 6.8 LTS with the WebEngine, WebChannel and Network modules.
- Windows uses MSVC 2022 64-bit. Qt WebEngine does not support MinGW.
- Development and verification happen on Windows. Changes must still build on macOS and Linux, so keep platform-specific code in `src/platform/`.
