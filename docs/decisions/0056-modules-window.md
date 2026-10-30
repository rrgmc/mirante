# 0056. A Modules window lists the modules and their item types

- Status: Accepted
- Date: 2026-10-29

## Context

The user asked for a menu entry that opens a window with every installed module and its item types, with a short description of each.

A build includes only the enabled [modules](../modules.md), so the item types on offer differ from build to build. Until now the app showed a module only in two places. The item dialog shows type names and icons, without saying what a type shows. The Profiles menu shows modules that have settings screens, and no others. Neither modules nor item types had a description.

## Decision

- **A description in the module interface.** `ItemType` gets `virtual QString description() const`, empty by default. `ModuleRegistrar` gets `setModuleDescription()`, which works like `setModuleName()`. `ItemTypeRegistry` keeps the module's description and returns it with `moduleDescription()`. Each description is one or two plain sentences that say what the module adds, or what an item of the type shows.
- **Every module in this repository has a name and a description, and so does each of its types.** The modules that had no name, such as notes and web, now call `setModuleName()`. Each module's register test checks that every type has a description.
- **Help > Modules…** opens the Modules window. It comes first in the Help menu, before About Mirante. Like the settings window, it is not modal: the menu brings an open window to the front.
- **The window.** On the left, a tree lists the modules by name, each with its item types under it, by name, with their icons. On the right, a read-only pane shows the selected entry. For a module it shows the description, the settings screens and where to edit them, and each item type with its icon, name and description. For a type it shows the icon, name, description and module. The first module is selected when the window opens.
- **Built-in types.** The core registers no item type today, so the window lists modules only. Built-in panels (`dash.panel`) will be listed under "Built in" when the core registers them.

## Alternatives

- **One long page with every module and type.** It shows everything at once, but it grows long, and a tree is easier to scan.
- **A tree with a description column.** Qt item views do not wrap long text well when a column is resized, so descriptions would be cut short.
- **Descriptions in the README of each module only.** The user would have to find them in the source. The window reads them from the build that runs.
- **Make `setModuleDescription()` optional with a default body.** `ModuleRegistrar` is implemented by the core only, so a pure virtual function costs modules nothing, and it matches `setModuleName()`.

## Consequences

- The user can see what each module of the build adds before creating items.
- A new item type needs a description to pass its module's tests, and a new module a name and a description, so the window stays complete.
- A module that gives no descriptions still builds; the window then shows its name and its types' names only.
