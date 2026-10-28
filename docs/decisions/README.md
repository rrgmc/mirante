# Decision records

Each file records one design decision: what was decided, what else was considered, and why. Records are not rewritten after they are accepted. A later change gets a new record that supersedes the old one, and the old one's status is changed to "Superseded by NNNN".

## Index

| No. | Decision | Status |
|---|---|---|
| [0001](0001-qt6-widgets.md) | Build with Qt 6 Widgets | Accepted |
| [0002](0002-own-layout-tree.md) | Own layout tree of splits, tabs and cells | Accepted |
| [0003](0003-view-per-placement.md) | One live view per placement | Accepted |
| [0004](0004-file-per-entity-sync.md) | One JSON file per entity in an externally synced folder | Accepted |
| [0005](0005-profiles-per-group.md) | One web login profile per group, with a shared option | Accepted |
| [0006](0006-connections-per-group.md) | API connections per group, tokens in the keychain | Accepted |
| [0007](0007-separate-item-types-per-rendering.md) | Separate item types for summary and page | Accepted |
| [0008](0008-claude-sessions.md) | Claude sessions through web views and the terminal only | Accepted |
| [0009](0009-priority-and-attention.md) | Four priority levels; attention as badges | Accepted |
| [0010](0010-folder-identity.md) | Folders stored as identities, mapped to paths per device | Accepted |
| [0011](0011-root-cells-reference-task-dashboards.md) | Root dashboard cells refer to task dashboards | Accepted |
| [0012](0012-html-prototype-first.md) | HTML prototype and Qt spike before features | Accepted |
| [0013](0013-compiled-in-modules.md) | Item types in compiled-in modules, chosen at build time | Accepted |
| [0014](0014-name-mirante.md) | Name the app Mirante | Accepted |

## Template

Copy this into `NNNN-short-title.md`, using the next free number.

```markdown
# NNNN. Title

- Status: Proposed | Accepted | Superseded by NNNN
- Date: YYYY-MM-DD

## Context

What problem or need led to this decision.

## Decision

What was decided, in a few sentences.

## Alternatives

Other options and why they were not chosen.

## Consequences

What becomes easier or harder because of this decision.
```
