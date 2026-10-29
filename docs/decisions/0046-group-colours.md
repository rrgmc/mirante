# 0046. Each group has a colour

- Status: Accepted; item cell headers by 0047
- Date: 2026-10-29

## Context

Task dashboard tabs and task cells in root dashboards all looked the same. Their title bars had a light tint of the highlight colour. Tasks from several groups share the tab bar and the root dashboards, so the user could not see at a glance which group a dashboard belongs to. The sidebar did not show it either, once the group's heading had scrolled away.

## Decision

**Data.** A group has an optional `colour`, stored in its file as `"#rrggbb"`. A group without one, or with one that is not of that form, takes a colour from a fixed palette of eight calm colours. The choice comes from an FNV-1a hash of the group's id, so it is the same on every device and every run, and existing groups get colours without their files changing. A new group takes the first palette colour that no group uses yet, counting those fallback colours, and stores it. When every colour is used, it takes the one used least. The palette leaves out red, orange and grey, since the sidebar uses them for priorities. The code is in `core/GroupColours`.

**Title bars.** The title bar of task dashboard tabs and item tabs, and the header of task cells in root dashboards, get a light tint of the task's group colour over the whole bar: 22% of the colour mixed into the window colour, the amount the highlight tint had. Mixing into the window colour keeps the bar light in a light palette and dark in a dark one, so the text stays readable in both. The tint follows at once when the group's colour changes, when the task moves to another project, and when its project moves to another group. A task cell whose task is removed keeps the highlight tint. Root dashboards belong to no group and keep their look.

**Item cells stay as they are.** Item cell headers inside a task dashboard keep their light highlight tint. The title bar above them already names the group. A second tint on every cell would compete with the module colours of the type pills, which tell items apart. Task cells nest item cells, and a root dashboard can hold item cells of any task, so tinted item headers would also mix several group colours in one view.

**Sidebar.** A group heading starts with a dot in the group's colour. Project rows have a light tint of their group's colour, 10%, instead of the highlight colour. The dot names the colour clearly next to the group's name. The project tint carries it down the list, so a project shows its group while the heading is scrolled away. Task rows stay plain, so the tree stays calm. The selection keeps the system highlight colour.

**Group dialog.** A row of palette swatches, a swatch for a colour outside the palette, and "Other…", which opens the system colour dialog. The current colour is checked. Opening and closing the dialog without choosing a colour leaves a group without a stored colour unchanged.

## Alternatives

- **A stripe or a pill in the group colour.** It marks the bar with less colour, but the user chose a tint of the whole bar, which is easier to see from a distance and fits the existing header tint.
- **Storing a colour in every existing group on first load.** It would write every group file on every device and could cause sync conflicts. The fallback by id gives the same colour without writing.
- **A palette index instead of a hex colour.** It is shorter, but a change to the palette would change every group's colour, and it cannot hold a colour from "Other…".
- **Tinting task rows in the sidebar too.** It shows the group on every row, but with priority markers and the selection it made the tree busy.

## Consequences

- Changing the palette or the hash changes the colour of every group without a stored colour. A test pins the fallback colours of the sample groups.
- A colour from "Other…" may be strong or dark. The 22% mix keeps the text readable, but the bar may look less calm.
- The core keeps colours as strings, so it still needs only Qt Core.
