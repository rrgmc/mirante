# 0033. Cell headers show the type and the place as pills

- Status: Accepted
- Date: 2026-10-29

## Context

An item cell's header showed the type icon, the item name and, in a root dashboard, the item's project and task in dimmed text. A task cell showed its title in bold and its project in dimmed text. The user found these headers inconsistent and hard to scan: the icon alone did not say what kind of item a cell holds, and the dimmed place text looked like part of the name.

## Decision

A cell header is built from small parts, left to right:

1. **Type pill.** A rounded background with the type's icon and a short label, such as "MR", "Issue", "Jira" or "Claude". The label comes from a new member of the module interface, `ItemType::shortName()`, which defaults to `name()`. Each built-in type sets one.
2. **Item name**, in normal weight.
3. **Place pill.** Only for an item from another task, that is, an item placed directly in a root dashboard. A grey pill with "project › task".

A task cell keeps its bold "Task / Dashboard" title, and its project moves into the same grey place pill.

**Colours.** The type pill's colour is chosen per module. The core mixes a colour of medium lightness and saturation into the header's background: 24% in a light palette and 38% in a dark one, where the same amount reads weaker. The text keeps the palette's text colour, so it stays readable in both. The built-in modules have fixed hues far enough apart to tell them apart:

| Module | Hue | Colour |
|---|---|---|
| `gitlab` | 25 | orange |
| `notes` | 50 | amber |
| `folder` | 120 | green |
| `web` | 185 | teal |
| `jira` | 215 | blue |
| `claude` | 270 | violet |
| `image` | 330 | rose |

Any other module gets one of twelve hues from a hash of its id, so a new module needs no core change. The place pill and the type pill of a missing module are neutral grey: the text colour mixed into the background, 10% in a light palette and 16% in a dark one. A missing module's pill shows the module id and the question-mark icon.

**Narrow cells.** When the header is too narrow, the place pill shrinks first, then the name, both cut with an ellipsis. Then the type pill drops its label and keeps its icon. Each part shows its full text as a tooltip.

## Alternatives

- **A colour member in the module interface,** such as `ItemType::accentColour()`. It lets a module choose, but every module would have to pick a colour that works in both palettes and next to the others. Keeping the colours in the core keeps them calm and consistent, and keeps the module interface small.
- **A colour per type instead of per module.** The types of one module, such as the GitLab MR and Issue types, would get unrelated colours. One colour per module groups them, and the label tells them apart.
- **Only the icon, with the full type name as a tooltip.** That was the old header, which the user found hard to scan.

## Consequences

- A module's types should set `shortName()` to one short word. Without it, the pill shows the full type name, which is long.
- The core holds a small table of hues for the built-in modules. A new built-in module works without an entry, but may share a hue with another module until one is added.
- Tests and code that look for header labels by object name keep working: the place pill keeps the names `cellContext` and `taskContext`.
