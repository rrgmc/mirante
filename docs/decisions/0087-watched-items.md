# 0087. Only watched items reach the tray and send notifications

- Status: Accepted
- Date: 2026-10-30

## Context

M8 puts an attention count on the tray icon and sends a desktop notification when a linked pipeline fails. Both reach the user outside the app, while they work in something else. If every item counted, a busy workspace would keep the tray badge lit, and notifications would come for work the user only keeps an eye on. The user wants to choose, item by item, what may reach them outside the app.

## Decision

- An item can be **watched**. A watched item counts on the tray icon and may send desktop notifications. Items that are not watched do neither.
- Every item starts **not watched**, also items made before this decision. The user opts in.
- Watching changes only the tray and notifications. The sidebar, Focus and cell headers show attention for every item, as before, so the dashboard never hides anything.
- The choice is stored in the item, as the field `watch`, and is synced. Every device of the workspace watches the same items.
- Watching is offered for item types that have a status, because only those report attention.
- A watched item in an active task is polled even when nothing shows it, so a failure is seen while the window is hidden.

## Alternatives

- **Watching hides every badge.** One switch would be simpler, but an unwatched item could then need attention and show nothing anywhere.
- **Two switches, for the count and for notifications.** It allows more choices than the user asked for, and makes the item dialog longer.
- **On by default for types that notify.** A new workspace would notify at once for every pipeline item, which is what the opt-in avoids.
- **A device setting.** Each device could choose, but the user would set every item again on each device.

## Consequences

- `Item` gains `watch`. Old versions keep it as an unknown key.
- The item dialog, and the context menus of items in the sidebar and in cell headers, offer "Watch".
- The status cache polls more items: the watched items of active tasks, at the normal interval.
- Without any watched item, the tray shows no count and no notification appears.
