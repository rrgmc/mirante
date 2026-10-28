# 0002. Own layout tree of splits, tabs and cells

- Status: Accepted
- Date: 2026-10-28

## Context

Dashboards must allow splits, tabs and cells nested in any combination: tabs inside cells inside tabs. The same engine serves task dashboards and root dashboards, where a cell can contain a whole task dashboard.

## Decision

Build a custom layout tree with three node types (split, tabs and cell), shown with nested `QSplitter` and `QTabWidget` widgets. Write the drag and drop with drop overlays in the app.

## Alternatives

- **Qt Advanced Docking System (LGPL)** and **KDDockWidgets (GPL or commercial).** Both are polished and provide docking, tab groups and floating windows. They are built around "splits of tab groups". Neither is known to support a full split layout inside one tab, which this design needs. Running a second docking manager inside a tab was not verified.

## Consequences

- The data model and the widgets match exactly, and the tree is simple to store as JSON.
- Drag and drop, drop overlays and normalisation have to be written and tested here. The docking libraries can still serve as a reference for how the overlays behave.
