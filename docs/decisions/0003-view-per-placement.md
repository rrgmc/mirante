# 0003. One live view per placement

- Status: Accepted
- Date: 2026-10-28

## Context

A Qt widget has one parent, so one web view can only appear in one place. The same item can be visible in two places at once, for example in two dashboards, or in a dashboard and a detached window.

## Decision

Each visible placement of an item gets its own view. Two placements of the same web item are two independent live pages in the same profile.

## Alternatives

- **One view per item that moves to wherever it is shown,** with a placeholder in the other places. It saves memory but makes the second place useless while the first is visible.

## Consequences

- Both places are fully usable at the same time.
- Memory use doubles for such items. The [memory policy](../web-views.md#memory-policy) limits hidden views.
- The two copies do not share scroll position, history or unsent input. Nothing is synchronised between them.
