# 0079. Modules declare device settings as fields

- Status: Accepted
- Date: 2026-10-30

## Context

Some values belong to one device, not to the workspace: the editor that opens a folder, the program that runs Claude Code. [Modules](../modules.md#what-a-module-registers) list "device settings" among the parts a module registers, and `ItemContext::deviceSetting()` was planned for M7, but nothing said how they are declared, edited or stored.

## Decision

- A module declares a device setting with `ModuleRegistrar::addDeviceSetting(field)`. The types Text, Integer, Boolean and Choice are allowed.
- The [settings window](../ui.md#settings) gets a Modules section, with one group per module that declares any, built from the fields as other forms are.
- Values are kept in `QSettings`, under `modules/<module id>/<key>`, with the other [device settings](../sync.md#device-settings). They are never synced.
- Item code reads them with `ItemContext::deviceSetting(key)`, which gives the field's default when nothing is stored.

## Alternatives

- **Settings screens with a "this device" flag.** Settings screens belong to profiles and sync with the workspace. Mixing the two would make it unclear which values travel.
- **One free-form command setting in the core.** Each module knows which commands it needs and what a good default is.

## Consequences

- Modules get device settings with no new widget code, through `FieldForm`.
- A module that is left out of a build keeps its stored values, because nothing deletes them.
- Settings for single items, such as remembered sections, are a different need and are not covered here.
