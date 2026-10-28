# 0012. HTML prototype and Qt spike before features

- Status: Superseded by [0015](0015-app-skeleton-instead-of-spike.md)
- Date: 2026-10-28

## Context

The navigation has many levels: groups, projects, tasks, items, task dashboards and root dashboards, with nested splits and tabs. It is the riskiest part of the design for usability, and expensive to change once built in Qt.

## Decision

- First, build a clickable HTML prototype with mock data ([M1](../roadmap.md#m1-html-prototype)). Refine the navigation with the user, and update [ui.md](../ui.md) with the result.
- Then build a Qt spike with read-only fixture data ([M2](../roadmap.md#m2-qt-spike-with-fixture-data)) to check the real feel of Qt's splitters and tabs.
- Only then build the features.

## Alternatives

- **Static mockup images.** Quick, but they cannot show how navigation feels.
- **Only one of the two prototypes.**

## Consequences

- Navigation changes cost minutes in the HTML prototype instead of a rewrite.
- The HTML prototype is thrown away. The Qt spike becomes the base of the app.
