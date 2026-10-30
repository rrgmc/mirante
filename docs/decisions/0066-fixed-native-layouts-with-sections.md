# 0066. Native views have fixed layouts, with sections that can be hidden

- Status: Accepted
- Date: 2026-10-30

## Context

M6 adds the native summary and list views. How much a person wants to see in them is a matter of taste. A full layout editor would need an editor window, a stored layout format and handling for every combination, which is too much for this project. Dashboards already let the user arrange cells with splits and tabs.

## Decision

- **Fixed layouts.** Each native type has one layout, in code. It is one column that wraps and scrolls, so a narrow cell loses no data.
- **A fixed top.** A summary always shows its title, its state and its action buttons, in a row under the title. They scroll with the rest of the view.
- **Sections that can be hidden.** The rest of a summary is a fixed sequence of named sections. Each has a boolean field in the item's configuration, on by default and also when the key is absent. The item dialog builds the form from those fields, so this adds no user interface and no core code.
- **The merge request list** has two such fields, for its pipeline and approvals columns.
- **Lists.** A list is a sortable table, with a preview of the selected row below a splitter. The preview stays hidden until a row is clicked. In a narrow cell the title column stays, and the other columns hide in a fixed order.

## Alternatives

- **A layout editor, with sections that can be reordered and moved.** This is where the cost jumps, and dashboards already cover most of the need.
- **No options at all.** Simple, but one person's useful field is another's clutter, and the fields cost almost nothing.
- **Remembering collapsed sections.** A change of configuration makes the view again and writes a synced file, which is wrong for a quick click. It would need device settings for items, which arrive in M7.
- **Saving column order and widths.** It has the same dependency on M7.

## Consequences

- The choice is per item and syncs, so an item the user reviews can show more than an item the user only watches.
- A hidden pipeline or approvals column saves requests. See [decision 0067](0067-lists-report-status.md).
- Order and position cannot be changed. A later decision can add that without changing stored configurations.
