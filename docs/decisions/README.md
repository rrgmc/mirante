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
| [0006](0006-connections-per-group.md) | API connections per group, tokens in the keychain | Superseded by 0054 |
| [0007](0007-separate-item-types-per-rendering.md) | Separate item types for summary and page | Accepted |
| [0008](0008-claude-sessions.md) | Claude sessions through web views and the terminal only | Accepted; local sessions superseded by 0084 |
| [0009](0009-priority-and-attention.md) | Four priority levels; attention as badges | Accepted; sorting superseded by 0034 |
| [0010](0010-folder-identity.md) | Folders stored as identities, mapped to paths per device | Accepted |
| [0011](0011-root-cells-reference-task-dashboards.md) | Root dashboard cells refer to task dashboards | Accepted |
| [0012](0012-html-prototype-first.md) | HTML prototype and Qt spike before features | Superseded by 0015 |
| [0013](0013-compiled-in-modules.md) | Item types in compiled-in modules, chosen at build time | Accepted |
| [0014](0014-name-mirante.md) | Name the app Mirante | Accepted |
| [0015](0015-app-skeleton-instead-of-spike.md) | Build the app skeleton directly, with CI from M2 | Accepted; macOS in CI by 0077 |
| [0016](0016-generic-items.md) | Generic items: any `QWidget`, with two view flags | Accepted |
| [0017](0017-parallel-tasks-with-fixed-interfaces.md) | Parallel tasks with fixed interfaces and globbed sources | Accepted; merging by 0109 |
| [0018](0018-no-default-task.md) | No Default task in projects | Accepted |
| [0019](0019-fixed-task-cells-and-item-windows.md) | Task cells show one dashboard, and items open alone | Accepted |
| [0020](0020-edit-task-dashboards-alone.md) | Task dashboards are edited only on their own | Accepted |
| [0021](0021-image-items-as-files.md) | Image items, with images stored as files | Accepted |
| [0022](0022-task-dashboard-tabs.md) | Task dashboards open in their own kind of tab | Accepted; place of the preview by 0048; pinning by 0052 |
| [0023](0023-item-tabs.md) | Items open alone in a tab | Accepted; place of the preview by 0048; pinning by 0052 |
| [0024](0024-detach-root-dashboard-tabs.md) | Only root dashboard tabs can be detached | Accepted |
| [0025](0025-workspace-file.md) | A workspace file holds the workspace's name | Superseded by 0026 |
| [0026](0026-workspace-file-name.md) | The workspace file is `workspace.mirante` | Superseded by 0027 |
| [0027](0027-workspace-needs-its-file.md) | A workspace is a folder with `workspace.mirante` | Accepted |
| [0028](0028-item-fields-in-m3.md) | Item types declare their fields, or bring their own editor, from M3 | Accepted |
| [0029](0029-sample-data-read-only.md) | The sample data is read-only | Accepted |
| [0030](0030-deleting-image-files.md) | Image files are deleted only when this device drops the last reference | Accepted |
| [0031](0031-conflict-copies-by-id-prefix.md) | Conflict copies are recognised by the id at the start of their name | Accepted |
| [0032](0032-paste-images-into-root-dashboards.md) | Pasting an image into a root dashboard asks for its task | Superseded by 0051 |
| [0033](0033-cell-header-pills.md) | Cell headers show the type and the place as pills | Accepted |
| [0034](0034-manual-order-in-the-sidebar.md) | The sidebar keeps the manual order; priority is a marker | Accepted |
| [0036](0036-trash-for-tasks-and-projects.md) | Deleted tasks and projects go to a trash | Accepted |
| [0037](0037-core-makes-web-views.md) | The core makes web views; modules may take the profile instead | Accepted; the item's profile by 0042 |
| [0038](0038-read-connections-in-m4.md) | Connections are read in M4, without tokens | Accepted; connections superseded by 0054 |
| [0039](0039-profiles-deleted-only-by-local-delete.md) | A group's web profile is deleted only when the group is deleted on this device | Accepted; for web profiles by 0042 |
| [0040](0040-profiles-per-group-and-module.md) | One web login profile per group and module | Superseded by 0042 |
| [0041](0041-own-look-for-tabs-and-tool-buttons.md) | Mirante draws its own tabs and tool buttons | Accepted; the current tab's colour by 0065 |
| [0042](0042-user-managed-web-profiles.md) | Web profiles are a list the user manages | Accepted; in part superseded by 0050 and 0057 |
| [0043](0043-pick-the-item-type-first.md) | A new item starts by picking its type in an icon grid | Accepted; the grid superseded by 0100 |
| [0044](0044-module-settings-in-the-workspace.md) | Modules have settings screens, stored in the workspace | Accepted; in part superseded by 0050 |
| [0045](0045-preview-and-crop-pasted-images.md) | Pasted images open a preview with a crop first | Accepted; pasting onto dashboards by 0051 |
| [0046](0046-group-colours.md) | Each group has a colour | Accepted; item cell headers by 0047 |
| [0047](0047-item-headers-in-group-colours.md) | Item cell headers take their group's colour | Accepted |
| [0048](0048-focus-and-preview-slots.md) | Focus and the preview have fixed slots right of the tabs | Accepted; pinning by 0052; the sidebar's Focus entry by 0053 |
| [0049](0049-items-from-a-pasted-link.md) | Item types read a pasted link, and the item dialog offers the types that take it | Accepted; connections superseded by 0054 |
| [0050](0050-profiles-hold-logins-connections-and-settings.md) | Profiles hold logins, connections and module settings | Accepted; connections superseded by 0054, Default by 0057 |
| [0051](0051-paste-images-from-the-items-menu.md) | Pasted images become items of a task, not cells of a dashboard | Accepted |
| [0052](0052-no-pinning.md) | The preview is never pinned | Accepted |
| [0053](0053-no-focus-entry-in-the-sidebar.md) | The sidebar has no Focus entry | Accepted |
| [0054](0054-module-settings-replace-connections.md) | Module settings replace connections | Accepted |
| [0055](0055-move-workspace.md) | Moving a workspace to another folder | Accepted |
| [0056](0056-modules-window.md) | A Modules window lists the modules and their item types | Accepted |
| [0057](0057-any-profile-can-be-the-default.md) | Any profile can be the default | Accepted |
| [0058](0058-one-instance-per-web-data-folder.md) | One Mirante process per web data folder; a later start hands over | Accepted |
| [0059](0059-logins-per-workspace.md) | Each workspace keeps its own logins | Accepted; a changed id by 0062 |
| [0060](0060-jira-cloud-or-data-center-by-setting.md) | Jira Cloud or Data Center is a setting, guessed from the site | Accepted |
| [0061](0061-attention-from-status-of-any-rendering.md) | Status comes from the item's object, and some attention clears when seen | Accepted |
| [0062](0062-logins-follow-a-new-workspace-id.md) | Logins follow a workspace id that another device changed | Accepted; tokens by 0063 |
| [0063](0063-tokens-follow-a-new-workspace-id.md) | Tokens follow a workspace id that another device changed | Accepted |
| [0064](0064-keychain-entries-start-with-mirante.md) | Keychain entries start with "mirante/" | Accepted |
| [0065](0065-current-tab-in-the-highlight-colour.md) | The current tab is filled with the highlight colour | Accepted |
| [0066](0066-fixed-native-layouts-with-sections.md) | Native views have fixed layouts, with sections that can be hidden | Accepted; the list preview by 0071 |
| [0067](0067-lists-report-status.md) | Lists report a status, without attention | Accepted |
| [0068](0068-views-read-what-the-user-asked-for.md) | Views ask the server directly only for what the user asked for | Accepted; the list preview by 0071 |
| [0069](0069-open-page-in-the-placement.md) | "Open page" is shown by the placement | Accepted |
| [0070](0070-read-mostly-native-views.md) | Native views are read-mostly | Accepted |
| [0071](0071-lists-link-out-without-a-preview.md) | Lists link out from each row, without a preview | Accepted |
| [0072](0072-pipelines-started-together.md) | A pipeline item shows the pipelines started together | Accepted |
| [0073](0073-jira-extra-fields.md) | Jira's extra fields are named in the profile's settings | Accepted |
| [0074](0074-refuse-workspaces-from-newer-versions.md) | A workspace from a newer version is not opened | Accepted |
| [0075](0075-hide-to-tray-and-show-hide-hotkey.md) | Closing hides to the tray, and a global hotkey shows or hides the app | Accepted; the default hotkey by 0076; closing by default by 0107 |
| [0076](0076-left-hand-default-hotkey.md) | The default global hotkey is pressed with the left hand | Accepted |
| [0077](0077-no-macos-in-ci-during-development.md) | No macOS in CI during development | Accepted; macOS back in CI in M10 |
| [0078](0078-quick-add-dialog.md) | Quick add is a dialog that reads text through the modules | Accepted; in part superseded by 0083 |
| [0079](0079-module-device-settings.md) | Modules declare device settings as fields | Accepted |
| [0080](0080-git-read-by-the-core.md) | The core reads Git state for modules | Accepted |
| [0081](0081-views-that-keep-their-own-keys.md) | A view can keep configuration keys it writes itself | Accepted |
| [0082](0082-commands-as-argument-lists.md) | Commands are passed as a program and its arguments | Accepted |
| [0083](0083-quick-add-into-a-task.md) | Quick add can add items to an existing task, and says which it does | Accepted |
| [0084](0084-no-local-claude-sessions.md) | No Claude session (local) item | Accepted |
| [0085](0085-drop-images-from-other-apps.md) | Images can be dropped from other apps | Accepted |
| [0086](0086-english-and-brazilian-portuguese.md) | The app speaks English and Brazilian Portuguese | Accepted |
| [0087](0087-watched-items.md) | Only watched items reach the tray and send notifications | Accepted |
| [0088](0088-keys-a-page-keeps.md) | App shortcuts work in web pages, and an item can give keys to its page | Accepted |
| [0089](0089-module-identity.md) | Modules give a UUID, a version, an author and a homepage | Accepted |
| [0090](0090-sign-the-macos-bundle.md) | Sign the macOS bundle with a certificate when one is set | Accepted; extended by 0091 |
| [0091](0091-default-macos-sign-identity.md) | The macOS build signs with the maintainer's certificate when the keychain has it | Accepted |
| [0092](0092-app-icon-vista.md) | The app icon is a sunrise over dashboard tiles | Superseded by 0096 |
| [0093](0093-dock-shows-the-bundle-icon.md) | The macOS Dock shows the bundle's icon | Accepted |
| [0094](0094-built-in-panels-as-a-core-item-type.md) | Built-in panels are one core item type, and see only their own task and group | Accepted |
| [0095](0095-the-bridge-gives-texts-and-colours.md) | The bridge gives panels their texts and colours | Accepted |
| [0096](0096-app-icon-arcada.md) | The app icon is a landscape seen through three arches | Superseded by 0097 |
| [0097](0097-app-icon-monte.md) | The app icon is an M drawn as two peaks under the sun | Accepted |
| [0098](0098-packages-from-cmake-install.md) | Packages are made by `cmake --install` and CPack, and take their version from the tag | Accepted |
| [0099](0099-linux-package-in-opt.md) | The Linux package installs into `/opt/mirante`, with its own Qt and an AppArmor profile | Accepted |
| [0100](0100-item-types-in-a-module-tree.md) | A new item picks its type in a tree of modules, with a description | Accepted |
| [0101](0101-macos-release-signing.md) | The macOS release is a universal bundle, signed with a Developer ID and notarised | Accepted; bundle identifier superseded by 0103 |
| [0102](0102-github-module.md) | A GitHub module, shaped like the GitLab module | Accepted |
| [0103](0103-bundle-identifier-rrgmc.md) | The macOS bundle identifier is `com.rrgmc.mirante` | Accepted |
| [0104](0104-document-items.md) | Document items show a file from a web address, the workspace or a project folder | Accepted |
| [0105](0105-sample-data-holds-available-items.md) | The sample data holds only items that work | Accepted |
| [0106](0106-start-at-sign-in.md) | Mirante can start at sign-in, hidden in the tray | Accepted |
| [0107](0107-closing-quits-by-default.md) | Closing the window quits by default | Accepted |
| [0108](0108-freeze-web-pages-in-the-tray.md) | Web pages freeze while the app is hidden to the tray | Accepted |
| [0109](0109-pull-requests-into-master.md) | Changes reach `master` only through pull requests | Accepted |

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
