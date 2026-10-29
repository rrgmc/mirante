# Decision records

Each file records one design decision: what was decided, what else was considered, and why. Records are not rewritten after they are accepted. A later change gets a new record that supersedes the old one, and the old one's status is changed to "Superseded by NNNN".

## Index

| No. | Decision | Status |
|---|---|---|
| [0001](0001-qt6-widgets.md) | Build with Qt 6 Widgets | Accepted |
| [0002](0002-own-layout-tree.md) | Own layout tree of splits, tabs and cells | Accepted |
| [0003](0003-view-per-placement.md) | One live view per placement | Accepted |
| [0004](0004-file-per-entity-sync.md) | One JSON file per entity in an externally synced folder | Accepted |
| [0005](0005-profiles-per-group.md) | One web login profile per group, with a shared option | Superseded by 0042 |
| [0006](0006-connections-per-group.md) | API connections per group, tokens in the keychain | Accepted |
| [0007](0007-separate-item-types-per-rendering.md) | Separate item types for summary and page | Accepted |
| [0008](0008-claude-sessions.md) | Claude sessions through web views and the terminal only | Accepted |
| [0009](0009-priority-and-attention.md) | Four priority levels; attention as badges | Accepted; sorting superseded by 0034 |
| [0010](0010-folder-identity.md) | Folders stored as identities, mapped to paths per device | Accepted |
| [0011](0011-root-cells-reference-task-dashboards.md) | Root dashboard cells refer to task dashboards | Accepted |
| [0012](0012-html-prototype-first.md) | HTML prototype and Qt spike before features | Superseded by 0015 |
| [0013](0013-compiled-in-modules.md) | Item types in compiled-in modules, chosen at build time | Accepted |
| [0014](0014-name-mirante.md) | Name the app Mirante | Accepted |
| [0015](0015-app-skeleton-instead-of-spike.md) | Build the app skeleton directly, with CI from M2 | Accepted |
| [0016](0016-generic-items.md) | Generic items: any `QWidget`, with two view flags | Accepted |
| [0017](0017-parallel-tasks-with-fixed-interfaces.md) | Parallel tasks with fixed interfaces and globbed sources | Accepted |
| [0018](0018-no-default-task.md) | No Default task in projects | Accepted |
| [0019](0019-fixed-task-cells-and-item-windows.md) | Task cells show one dashboard, and items open alone | Accepted |
| [0020](0020-edit-task-dashboards-alone.md) | Task dashboards are edited only on their own | Accepted |
| [0021](0021-image-items-as-files.md) | Image items, with images stored as files | Accepted |
| [0022](0022-task-dashboard-tabs.md) | Task dashboards open in their own kind of tab | Accepted |
| [0023](0023-item-tabs.md) | Items open alone in a tab | Accepted |
| [0024](0024-detach-root-dashboard-tabs.md) | Only root dashboard tabs can be detached | Accepted |
| [0025](0025-workspace-file.md) | A workspace file holds the workspace's name | Superseded by 0026 |
| [0026](0026-workspace-file-name.md) | The workspace file is `workspace.mirante` | Superseded by 0027 |
| [0027](0027-workspace-needs-its-file.md) | A workspace is a folder with `workspace.mirante` | Accepted |
| [0028](0028-item-fields-in-m3.md) | Item types declare their fields, or bring their own editor, from M3 | Accepted |
| [0029](0029-sample-data-read-only.md) | The sample data is read-only | Accepted |
| [0030](0030-deleting-image-files.md) | Image files are deleted only when this device drops the last reference | Accepted |
| [0031](0031-conflict-copies-by-id-prefix.md) | Conflict copies are recognised by the id at the start of their name | Accepted |
| [0032](0032-paste-images-into-root-dashboards.md) | Pasting an image into a root dashboard asks for its task | Accepted |
| [0033](0033-cell-header-pills.md) | Cell headers show the type and the place as pills | Accepted |
| [0034](0034-manual-order-in-the-sidebar.md) | The sidebar keeps the manual order; priority is a marker | Accepted |
| [0036](0036-trash-for-tasks-and-projects.md) | Deleted tasks and projects go to a trash | Accepted |
| [0037](0037-core-makes-web-views.md) | The core makes web views; modules may take the profile instead | Accepted; the item's profile by 0042 |
| [0038](0038-read-connections-in-m4.md) | Connections are read in M4, without tokens | Accepted |
| [0039](0039-profiles-deleted-only-by-local-delete.md) | A group's web profile is deleted only when the group is deleted on this device | Accepted; for web profiles by 0042 |
| [0040](0040-profiles-per-group-and-module.md) | One web login profile per group and module | Superseded by 0042 |
| [0041](0041-own-look-for-tabs-and-tool-buttons.md) | Mirante draws its own tabs and tool buttons | Accepted |
| [0042](0042-user-managed-web-profiles.md) | Web profiles are a list the user manages | Accepted |
| [0045](0045-preview-and-crop-pasted-images.md) | Pasted images open a preview with a crop first | Accepted |

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
