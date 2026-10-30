# 0015. Build the app skeleton directly, with CI from M2

- Status: Accepted; macOS in CI superseded by 0077
- Date: 2026-10-28

## Context

[Decision 0012](0012-html-prototype-first.md) planned two steps before the features: an HTML prototype (M1), then a Qt spike with read-only fixture data (M2), "to check the real feel of Qt's splitters and tabs".

M1 is done. The user tried the navigation in the prototype and agreed the changes, which are now in [ui.md](../ui.md). A separate stop to judge the feel in Qt adds little after that. Problems specific to Qt Widgets, such as how the drop zones look over a `QSplitter`, appear while the layout view is built, and cost the same to fix then.

The same decision already said the spike becomes the base of the app. Its work list is the app's foundation: the build, the modules, the model, the layout engine and the main window.

The repository is now public, so GitHub Actions builds on Windows, macOS and Linux cost nothing. Until now the roadmap checked macOS and Linux by hand until a CI build in M10.

## Decision

- There is no Qt spike. M2 is renamed "App skeleton and layouts" and builds the base of the app directly. It has no separate step for judging the feel of Qt.
- M2 still reads a fixed set of JSON files and writes nothing back. Writing, reloading and conflicts stay in M3. The fixture files are also the test data.
- M2 adds a GitHub Actions build for Windows, macOS and Linux that runs the tests, and a build with all modules off. These move from M10 to M2. M10 keeps packaging.
- The HTML prototype part of decision 0012 stands, and is complete.

## Alternatives

- **Keep the spike as planned.** It adds a review stop that the HTML prototype already made unnecessary.
- **Merge M2 and M3,** so the first Qt milestone also writes data. The milestone would become too large to review as one piece.
- **Add CI in M10, as planned.** macOS and Linux breakage would be found late, and checking by hand is easy to skip.

## Consequences

- M2 code is written to last from the start. There is no phase in which shortcuts are acceptable because the code is temporary.
- Every push from M2 onwards is built and tested on three platforms by CI.
- CI uses only the Qt modules a milestone needs. Qt WebEngine joins the CI build in M4.
