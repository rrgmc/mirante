# 0081. A view can keep configuration keys it writes itself

- Status: Accepted
- Date: 2026-10-30

## Context

A placement makes its view again whenever the item's configuration changes, so a view always shows what is stored. The Notes item keeps its text in its configuration, and saves while the user types. Each save would make the editor again, and lose the cursor, the selection and the undo history.

## Decision

- An item type can return keys from `viewKeys()`. Its view writes them with `updateConfig()`.
- When only those keys change, the placement keeps the view and calls the receivers the view registered with `ItemContext::onConfigChanged()`. Any other change makes the view again, as before.
- The view decides what to do with a change it did not make. The Notes view takes the new text unless it has an unsaved change.

## Alternatives

- **Notes in a file of their own.** It would avoid the configuration, but adds a second kind of synced file per item, with its own conflicts.
- **The placement never makes a native view again.** Every native view would have to follow every change of its configuration, which the summaries do not need.
- **Save only when the editor loses focus.** Text would be lost if the app stopped, and other devices would see it late.

## Consequences

- Types without `viewKeys()` behave as before.
- A view with view keys must handle changes from other devices itself.
