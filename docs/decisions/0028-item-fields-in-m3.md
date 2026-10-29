# 0028. Item types declare their fields, or bring their own editor, from M3

- Status: Accepted
- Date: 2026-10-28

## Context

M3 lets the user create and edit items. An item's configuration depends on its type, such as a URL for a web page or a project path and number for a merge request. The module interface planned `Fields.h` for M5, together with connection kinds and project settings.

Most types need only a few plain fields. Some may need an editor that a list of fields cannot describe, such as one that looks up values while the user types.

The image module also needs a way to say that a pasted image creates one of its items, without the core naming the module.

## Decision

- Each module chooses, for each of its item types, how its configuration is edited:
  - **Typed fields.** `fields()` returns a list of fields, and the core builds a form from it and checks the values. A field has a key, a label, a type, a default, a required flag and, for choices, its values and labels. The types are text, multi-line text, URL, integer, boolean, choice, folder and image.
  - **Its own editor.** `createEditor()` returns an `ItemEditor`, a widget that takes the configuration, returns the edited configuration, and reports what is invalid.
- `createEditor()` returns null by default. When it returns an editor, the dialog uses it; otherwise the dialog builds the form from `fields()`.
- `Fields.h` and `ItemEditor.h` join the module interface in M3.
- `ItemType` also gains `pastedImageField()`, the key of the image field that a pasted image fills, empty by default.
- Keys that the form or editor does not show are kept unchanged.
- Connection kinds and project settings use the same `Field` in M5.

## Alternatives

- **Typed fields only.** Less code in the core, but a type that needs a richer editor would have to wait for the field types to grow.
- **Own editors only.** Every module would repeat the same form code for plain fields.
- **Edit the configuration as raw JSON in M3.** Quick to build, but every item would need hand-written JSON until M5, and the dialog would be thrown away.
- **A fixed "image" type id known to the core for pasting.** Simple, but the core would name a module, which [decision 0013](0013-compiled-in-modules.md) rules out.

## Consequences

- Every module declares fields or an editor for its types in M3, from the configuration listed in [item types](../item-types.md).
- An editor gets what a form gets: the project's folders and a way to save an image into `files/`, through an `EditorContext`.
- The URL paste of M6 fills a configuration object, which both kinds of editor then show.
- An item of a missing module has neither. Its dialog shows the name only and keeps its configuration.
