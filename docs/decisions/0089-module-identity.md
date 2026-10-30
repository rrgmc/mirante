# 0089. Modules give a UUID, a version, an author and a homepage

- Status: Accepted
- Date: 2026-10-30

## Context

The user asked that each module have an author, a version, a homepage link and a unique UUID.

Until now a module had only an id, a name and a description ([decision 0056](0056-modules-window.md)). The id is short and readable, but two people could pick the same one, and it could be renamed. A version says which state of a module a build holds, and an author and a homepage say who made it and where to read about it. Modules are compiled in ([decision 0013](0013-compiled-in-modules.md)), so all of this is fixed in the module's code.

## Decision

- **Four setters in the module interface.** `ModuleRegistrar` gets `setModuleUuid(QUuid)`, `setModuleVersion(QString)`, `setModuleAuthor(QString)` and `setModuleHomepage(QUrl)`. They work like `setModuleName()`. `ItemTypeRegistry` keeps the values and returns them with `moduleUuid()`, `moduleVersion()`, `moduleAuthor()` and `moduleHomepage()`.
- **The registry checks them.** As with a bad item type id, the program stops with `qFatal()` when a module sets a null UUID, a UUID that another module already set, a version that is not numbers separated by dots (such as `1.2.0`), or a homepage that is not an `http` or `https` address.
- **The UUID never changes.** It names the module across renames of its id or name. A new module takes a newly generated UUID; it is never copied from another module.
- **Every module in this repository gives all four.** Each starts at version `0.1.0`, with Rangel Reale as author and its folder on GitHub as homepage. Each module's register test checks that all four are set.
- **The Modules window shows them.** The pane of a module lists its version, author, homepage and UUID under its description, each on its own line and only when the module gave it. The homepage is a link that opens in the system's browser.

## Alternatives

- **Arguments of `mirante_add_module()` in CMake.** CMake could refuse a module without them at configure time. But the name and description are already set in the register function, so the module's identity would be split across two files, and the generated `enabled_modules.cpp` would grow a second channel into the registry.
- **One `setModuleInfo()` call with a struct.** It is one call instead of four, but it differs from `setModuleName()` and `setModuleDescription()`, and a struct in the module interface is harder to extend without breaking modules.
- **Make them required at registration.** Tests register small modules of their own, and a module without them still works. The register test of each real module checks them instead, as it checks descriptions.
- **Use the app's version for every module.** Modules could then not say that one of them changed while the app did not.

## Consequences

- A new module needs a UUID, a version, an author and a homepage to pass its register test.
- Two modules with the same UUID stop the app at start, so a copied module is caught on first run.
- A later feature, such as modules loaded at run time, can identify a module by its UUID.
- Module versions are raised by hand when a module changes in a way worth noting.
