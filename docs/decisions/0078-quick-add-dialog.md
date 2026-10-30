# 0078. Quick add is a dialog that reads text through the modules

- Status: Proposed
- Date: 2026-10-30

## Context

[Quick add](../ui.md#quick-add) makes a task from a pasted Jira key or merge request link. [Modules](../modules.md#quick-add) say that handlers recognise the text and finders offer related items, so that `gitlab` and `jira` work together without depending on each other. Three things were left open:

- Reading a key or a link needs the server, and a server belongs to a [profile](../data-model.md#profile). Before a project is chosen, there is no group, and so no profile.
- The handlers and finders call servers, so they answer late, in any order, or not at all.
- [Decision 0018](0018-no-default-task.md) says quick add asks for a task or creates one. A new task gets one dashboard with an empty cell, so the new items would not show.

## Decision

- **A dialog.** The user pastes text, sees the proposed title, project and items, changes them, and presses Create. Nothing is created before that.
- **A profile in the dialog.** It starts as the profile of the group selected in the sidebar, else the workspace default, and the user can change it. Handlers and finders use its servers. The new items use the group's profile, as any new item does.
- **Asynchronous requests.** Handlers and finders get a request object, like a status job, and call `finish()` once. The dialog shows answers as they arrive. A handler that takes longer than 30 seconds counts as declining.
- **A new task, always.** Quick add creates a task in the chosen project. Its dashboard gets one cell that holds every item as a tab. The task opens in the preview slot.
- **Several readings.** When more than one handler takes the text, the dialog lets the user choose.

## Alternatives

- **Try every profile.** A link names its server, but a bare Jira key does not, and asking every profile's server sends the key to servers it does not belong to.
- **Add to an existing task.** Possible later. The item dialog already makes one item from a link ([decision 0049](0049-items-from-a-pasted-link.md)), which covers the common case.
- **A split per item in the dashboard.** Three or four items in one cell, as tabs, fit any window. The user rearranges them in the dashboard editor.

## Consequences

- The `QuickAdd.h` interface follows the pattern of `StatusJob`, so module authors learn one pattern.
- A key from a second Jira site needs a second profile, chosen in the dialog.
- The dialog has to handle partial answers and cancelling, which its tests cover with fake handlers.
