# 0043. A new item starts by picking its type in an icon grid

- Status: Accepted
- Date: 2026-10-29

## Context

The item dialog chose the type of a new item in a combo box above the name and the configuration. A combo box shows one type at a time, so the user had to open it and read a long list to see what the build offers. The configuration below changed each time the type changed. Each module may bring its own editor for its types ([decision 0028](0028-item-fields-in-m3.md)), so the rest of the dialog can look very different from one type to the next.

## Decision

Creating an item takes two steps in one dialog with two pages.

1. **Choose a type.** A grid of large icons, with each type's name under its icon, like the icon view of a file manager. The grid wraps, so many types show at once. It lists the registered types by name. Next, a double click or Enter goes on.
2. **Edit the item.** The name, the web profile for web types, and the type's own editor or the form built from its fields. The type is shown, not chosen. Back returns to the grid.

Choosing another type after Back starts from that type's defaults. Choosing the same type keeps what was entered. Editing an existing item shows only the second page, as before; its type cannot change.

## Alternatives

- **A separate type picker, then the item dialog.** It needs two dialogs in a row, and so two calls to the dialog runner that `EntityActions` uses, which tests replace. Going back to the grid would mean closing one dialog and opening the other.
- **Keep the combo box, with larger icons.** It still shows one type at a time.
- **Group the grid by module, with headings.** A list view in icon mode has no headings. Type names start with their product, such as "GitLab" or "Jira", so sorting by name already keeps a module's types together.

## Consequences

- The user sees every type at once, and the second page only ever shows one type's editor.
- Item types have no description yet. The grid shows the name, and a tooltip repeats it for names that are cut short. A description can be added to `ItemType` later and shown under the grid.
- Pasting a URL to pick the type and fill its fields, planned in the [roadmap](../roadmap.md), fits the first page.
