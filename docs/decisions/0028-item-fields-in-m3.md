# 0028. Item types declare their fields from M3

- Status: Proposed
- Date: 2026-10-28

## Context

M3 lets the user create and edit items. An item's configuration depends on its type, such as a URL for a web page or a project path and number for a merge request. The module interface planned `Fields.h` for M5, together with connection kinds and project settings.

The image module also needs a way to say that a pasted image creates one of its items, without the core naming the module.

## Decision

- `Fields.h` joins the module interface in M3. A field has a key, a label, a type, a default, a required flag and, for choices, its values and labels. The types are text, multi-line text, URL, integer, boolean, choice, folder and image.
- `ItemType` gains `fields()`, empty by default, and `pastedImageField()`, the key of the image field that a pasted image fills, empty by default.
- The core builds the item dialog from the fields and validates the values. Keys not declared are kept unchanged.
- Connection kinds and project settings use the same `Field` in M5.

## Alternatives

- **Edit the configuration as raw JSON in M3.** Quick to build, but every item would need hand-written JSON until M5, and the dialog would be thrown away.
- **Each module supplies its own editor widget.** More freedom, but each module repeats the same form code, and the URL paste of M6 would need every editor to support it.
- **A fixed "image" type id known to the core for pasting.** Simple, but the core would name a module, which [decision 0013](0013-compiled-in-modules.md) rules out.

## Consequences

- Every module declares its fields in M3, from the configuration listed in [item types](../item-types.md).
- An item of a missing module has no fields. Its dialog shows the name only and keeps its configuration.
- M5 reuses the form for connections and project settings.
