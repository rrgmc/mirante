# notes module

Markdown notes, stored in the task file, as an item. See [item types](../../../docs/item-types.md#notes).

## Item types

| Id | Name | Rendering | Description |
|---|---|---|---|
| `notes` | Notes | native | A Markdown note with a preview. Its text is kept in the task's file. |

The module calls itself "Notes" and describes itself as: "Markdown notes, kept in the task's file, so they sync with the workspace." Help > Modules shows the names and descriptions. See [decision 0056](../../../docs/decisions/0056-modules-window.md).

Its UUID is `27bc56ed-94f4-43f0-85be-623c723b1bb0`. It is at version 0.1.0, by Rangel Reale. Help > Modules also shows these, and the module's homepage. See [decision 0089](../../../docs/decisions/0089-module-identity.md).

The type has an id, a name and an icon, and declares that it is native. Since M7 it has a view.

## View

`NotesView.cpp` makes the view.

- A thin bar holds an **Edit** and a **Preview** button, with a stack of the two modes below. The view opens in Preview when the note has text, else in Edit. `Ctrl+E` switches between them. The shortcut works only while the keyboard's focus is in the view, so two notes on screen each switch alone.
- Edit is a plain text editor with the system's fixed-width font.
- Preview shows the text as Markdown, in the view kit's `MarkdownView`, with no fetcher. It loads nothing: a remote image or a local file named in the text shows as nothing. A click on a web or mail link opens it in the external program.

### Saving

The view writes the text into the configuration itself, with `ItemContext::updateConfig()`, keeping the other keys. It saves 800 ms after the last change, when the editor loses the keyboard's focus, when the user switches to Preview, and when the view is destroyed with unsaved changes. Empty text removes the key.

The type returns `text` from `viewKeys()`, so a save does not make the view again ([decision 0081](../../../docs/decisions/0081-views-that-keep-their-own-keys.md)). The view registers with `ItemContext::onConfigChanged()` instead. A change from another device replaces the text only when the view has no unsaved change, and keeps the mode. Otherwise the local text wins when it is saved.

## Configuration

| Key | Type | Description |
|---|---|---|
| `text` | string | The note's Markdown text. Missing when the note is empty. |

The type declares no fields, so the item dialog edits only the name, and keeps `text` unchanged. The note's text is edited in the view.

## Settings

None in M2.
