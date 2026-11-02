# 0118. All builds are the same app

- Status: Accepted
- Date: 2026-11-02

## Context

Several names fix which app a build is: the bundle identifier, the organisation and application name, the keychain service, the package name, the install folder and the desktop file name. See [the custom builds research](../research/custom-builds.md#app-identity). A custom build had to either keep these names and replace Mirante, or change them and stand beside it. The roadmap left this open.

The workspace is not part of this choice. A workspace is a folder with `workspace.mirante`, and any build can open it. What the names separate is the app's local state: its device settings, tokens and web logins.

## Decision

- **Every build is the app `mirante`,** with the official names, including the official app and any custom build. The manifest has no app id.
- **Installing a custom build replaces the installed Mirante,** and the other way round. It keeps the device settings, tokens and web logins.
- **The About dialog tells builds apart,** by showing each module's source and commit.

## Alternatives

- **A custom build is its own app.** It installs beside Mirante, with its own local state. This suits someone who wants two builds at once, but few people need that, and everyone else would enter their tokens and sign in again. It would also not protect the tokens from a malicious module, which can ask the keychain for the official service by name; see [decision 0121](0121-modules-are-trusted-code.md). All the names would also have to become configurable and be tested.
- **The builder chooses.** This is the most flexible, but both paths would have to be built, tested and documented. An optional app id in the manifest can still be added later, without breaking anything, if a real need appears.

## Consequences

- The core needs no configurable names.
- Moving to a custom build needs no new setup, except that the macOS keychain asks again for each token if the build's certificate differs.
- Two builds cannot be installed at once, and a second start hands over to the running app.
- If an install removes a module, its items show as placeholders and their data is kept. See [missing modules](../modules.md#missing-modules).
- A build with an older core refuses a workspace that has a newer `schemaVersion`, as in [decision 0074](0074-refuse-workspaces-from-newer-versions.md).
- The sample cannot rely on its names to stay away from real data. See [decision 0116](0116-custom-builds-in-separate-repositories.md).
