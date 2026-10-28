# 0013. Item types in compiled-in modules, chosen at build time

- Status: Accepted
- Date: 2026-10-28

## Context

Other people should be able to contribute item types, and anyone should be able to build a version of rraidash with only the types they want. Item types still need the configuration the core owns: connections and their tokens, project defaults, the status cache and web profiles.

Loading item types as external Qt plugins was considered and rejected. See the alternatives.

## Decision

- Item types live in **modules**. A module is one folder in `src/modules/<id>/`. It holds a family of item types and everything they share: connection kinds, project settings, API clients, attention rules, quick add handlers and tests.
- Modules are compiled into the app. Each module has a CMake option, `RRAIDASH_MODULE_<ID>`, that includes or leaves it out. The unit is the module, not the single type.
- CMake generates the code that registers the enabled modules. Modules do not register themselves through static objects.
- Modules use only a small module interface. They never include other core headers.
- The core never names a service. It contains no GitLab or Jira code, and it reaches modules only through registries.
- Modules reach connections through the context the core gives them:
  - the core signs requests for a module with the connection's token;
  - a separate, explicit call returns the raw token, for the rare cases that need it, such as passing it to a terminal.
- Items, connections and settings that belong to a module missing from the build are kept unchanged and shown as placeholders.
- The core is shaped this way before M2, so no service-specific code needs to be moved later.

## Alternatives

- **External Qt plugins loaded at runtime.**
  - A plugin must match the app's Qt version and compiler, and the module interface would have to stay binary compatible.
  - Plugins run in the app process, so a crash takes down the app, and a plugin can read every secret.
  - Plugins cannot be unloaded safely, and macOS refuses unsigned libraries from other teams.
  - It would also need one build per platform and architecture.
- **An option for each item type.** It allows finer choices, such as keeping "GitLab MR (summary)" without "GitLab MR (page)". It also gives many more options to maintain for little gain.
- **Item types in the core, with `#ifdef` switches.** It needs no module interface, but contributors would have to change core files, and the switches would spread through the code.
- **Raw tokens for all modules.** It is simpler, but every module would have its own authentication code, and tokens would reach far more code.

## Consequences

- A contributor adds a service by adding one folder. No core file changes.
- A build can leave out any module, including GitLab and Jira.
- Synced data can reach a build without the matching module. Placeholders and unchanged write-back are required, not optional.
- Project defaults such as the GitLab project path move from fixed project fields to module settings.
- Quick add, attention and the "suggest done" rule become generic, driven by what the modules register.
- Connection kind ids start with the module id, so `jira-cloud` and `jira-dc` become `jira.cloud` and `jira.dc`.
- The module interface becomes a contract. Changing it means changing every module, so it is kept small.
