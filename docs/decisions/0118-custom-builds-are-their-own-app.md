# 0118. A custom build is its own app by default

- Status: Accepted
- Date: 2026-11-02

## Context

Several names fix which app a build is: the bundle identifier, the organisation and application name, the keychain service, the package name, the install folder and the desktop file name. See [the custom builds research](../research/custom-builds.md#app-identity). A custom build had to either replace Mirante, sharing its settings, workspace and tokens, or stand beside it. The roadmap left this open.

## Decision

- **The manifest gives an app id and a display name,** and every name above follows from them. A custom build installs beside Mirante, with its own settings, workspace and tokens.
- **Only the official app uses the official names.**
- **`mirante-app` has an identity of its own during the sample,** so it cannot change the user's real data.

## Alternatives

- **A custom build replaces Mirante.** This is simpler for someone who wants Mirante with one more module, but a third-party module would run with the official app's tokens, and the sample would change the user's real data.
- **The builder chooses.** This is the most flexible, but both paths would have to be built, tested and documented.

## Consequences

- The names listed above must become configurable in the core.
- Normal module code in a custom build does not find the official app's tokens. This is not a barrier against malicious code; see [decision 0121](0121-modules-are-trusted-code.md).
- Someone who moves to a custom build enters their tokens again and copies or recreates their workspace. So does the user during the daily use of the sample.
