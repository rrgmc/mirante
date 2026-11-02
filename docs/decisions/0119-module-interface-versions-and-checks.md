# 0119. The module interface has a version, compatibility rules and checks

- Status: Accepted
- Date: 2026-11-02

## Context

All 45 commits that changed `src/module_api` by 2 November were made between 28 and 31 October, so the interface has no record of being stable. Once it is public, a change can break modules in other repositories. Modules are always compiled with the app, so they need source compatibility, not binary compatibility.

## Decision

**Versions.**

- The interface has a version, `major.minor`, available in CMake and as a C++ macro. Each module states the version it needs. A mismatch stops configuration with a message that names the module and points to the interface changelog.
- It is published as 0.x. Any minor version may break modules, so the check requires an exact match.
- It becomes 1.0 when a module from outside the core repository builds and works, and the interface has settled. From then on, a minor version only adds things, and only a major version may break them.

**Compatibility rules.**

- A new method on a class that modules implement, such as `ItemType`, has a default that keeps the old behaviour. It is never pure virtual.
- A changed signature is a new method whose default calls the old one, as `fieldsForProfile()` calls `fields()`.
- A new member of a struct, such as `Field`, goes last and has a default value.
- A large new feature is a small interface of its own, registered with `ModuleRegistrar` where possible.
- A removal is first marked `[[deprecated]]` with the replacement in its message, and happens only in a new major version.

**Checks.**

- CI fails if a pull request changes `src/module_api` without an entry in the interface changelog.
- The core's CI builds the first-party modules, so the split does not hide a break.
- A small module that implements only the required methods is kept unchanged, and CI builds it.
- Modules are built with `-Werror=suggest-override` on GCC and Clang. Without `override`, a changed signature leaves a method that still compiles but is never called.
- External modules are not built with `MIRANTE_WERROR`, so a new enum value or a newer compiler's warning does not stop an old module from building.
- Once outside authors exist, the template offers a scheduled build of a module against the latest core.

**Work before publishing the interface.**

- Remove what no module uses: `ItemContext::profile()`, `ItemContext::itemName()`, `ApiClient::secret()`, `ApiClient::authorize()`, `ItemType::singleView()` and `ItemType::reparentSafe()`. Any of them can return when a module needs it.
- Remove the planned `Values.h` and the planned `item()`, `task()`, `project()` and `group()` from the documentation. Correct the documentation's section on attention, which does not match `Status.h`.
- Add a `FakeItemContext` and a registration test helper to the testing library, so that module tests stop using `items/ItemTypeRegistry.h`, `items/FieldValidation.h` and `mirante_items`, and stop implementing `ItemContext` by hand.
- Set a naming rule for third-party module ids, such as an owner prefix. Ids are stored in users' data, so they are permanent.
- Make `DEPENDS` link the other module and make its headers available.

**Place.** The interface stays in `mirante-core`, in a folder that also configures on its own. A module can then compile and run its tests alone by fetching `mirante-core` with `SOURCE_SUBDIR`. This folder also holds `mirante_add_module()` and the helpers it needs.

## Alternatives

- **COM-style interfaces with a query method.** They exist so that code compiled at different times can work together. Modules are always recompiled, and defaulted methods give the same result. `dynamic_cast` is enough if a feature ever needs an optional interface on an object.
- **ABI or API comparison tools.** These matter for prebuilt binaries. The changelog check gives most of the benefit.
- **Checks at run time.** Modules are compiled in, so configuration is early enough.
- **A separate repository for the interface.** Each change would take one more step, and the core's pin and a module's pin could differ. The folder can move later with `git subtree split`, and authors would change only a URL.
- **No version until the interface settles.** Authors would get only compiler errors, without knowing why.

## Consequences

- The checks report a break but do not reduce the work of fixing it. Behaviour that changes but still compiles is caught only by a module's own tests.
- While the interface is 0.x, an exact match means that an app must move its modules and the core together.
- A later binary interface, for prebuilt modules, would need further rules.
