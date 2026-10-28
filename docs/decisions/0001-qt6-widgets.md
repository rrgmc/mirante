# 0001. Build with Qt 6 Widgets

- Status: Accepted
- Date: 2026-10-28

## Context

rraidash shows many web pages, such as claude.ai, GitLab and Jira, next to native panels. The user arranges them in nested splits and tabs. It must run on Windows, macOS and Linux.

## Decision

Use C++17 with Qt 6.8 LTS, Qt Widgets and Qt WebEngine, built with CMake.

## Alternatives

- **GPUI with gpui-component (Rust).** It is fast and has small binaries. But it is pre-1.0 with a changing API, and its web view is a native child window placed over the GPUI window. Nothing can be drawn above a web view, so drop overlays and menus over web content would not work.
- **Plain wry with tao or winit (Rust).** The same overlay problem, and there is no widget toolkit at all, so every splitter and tab bar would be custom.
- **Tauri.** Mature for "app that hosts web views", but third-party sites such as claude.ai cannot be shown in iframes, so the layout would still need native child web views with the same overlay problem.
- **QML / Qt Quick.** Possible, but splitters and docking-style layouts are more work than with Widgets.

## Consequences

- `QSplitter`, `QTabWidget` and `QWebEngineView` cover most of the layout engine directly, and overlays work above web content.
- The app ships Qt WebEngine, about 100 MB or more. That is accepted.
- On Windows, the build must use MSVC, because Qt WebEngine does not support MinGW.
