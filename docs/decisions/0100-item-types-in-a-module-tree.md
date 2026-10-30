# 0100. A new item picks its type in a tree of modules, with a description

- Status: Accepted
- Date: 2026-10-30

## Context

[Decision 0043](0043-pick-the-item-type-first.md) chose the type of a new item in a grid of large icons, sorted by name. Since then, modules and item types have descriptions, and the Modules dialog ([decision 0056](0056-modules-window.md)) shows them in a tree of modules with their types, beside a details pane. The grid could show neither: it had no headings, and its cells had room for the name only. Types such as "GitLab MR (page)" and "GitLab MR (summary)" differ by a word, and the grid did not say how they differ.

## Decision

The type page of the item dialog shows the types as the Modules dialog does.

- **A tree on the left.** Each module is a bold row, expanded, with its types under it, each with its small icon. Without a link, the built-in module comes first, then the others by name, and each module's types by name. With a link, the tree shows only the types that take it, and the module of the best match comes first.
- **A details pane on the right.** It shows the current type's icon, its name and its description. It shows only what describes the type: not its module, id or fields. On a module row, it shows the module's name and description.
- **Choosing.** A module row is not a type, so Next is disabled on it and a double click opens or closes it. A double click on a type, Enter or Next goes on, as before.

The second page, Back and the pasted link are unchanged.

## Alternatives

- **Keep the grid, with the description under it.** The grid still has no headings, and a pane under it takes height from the grid.
- **A flat list when a link is pasted, best first.** The order of the matches would be kept exactly, but the page would change shape as the user types. A link usually matches one module and Web page, so grouping by module keeps the best match first.

## Consequences

- The user sees which module each type comes from, and how similar types differ, before choosing.
- The type page and the Modules dialog look alike, so the user learns one layout.
- Large icons are no longer shown when choosing a type. The details pane shows the current type's icon at 32 pixels.
- A type without a description shows only its name in the pane. Modules should give their types descriptions.
