# 0103. The macOS bundle identifier is `com.rrgmc.mirante`

- Status: Accepted
- Date: 2026-10-30

## Context

[Decision 0101](0101-macos-release-signing.md) set the bundle identifier to `com.rangelreale.mirante`, after favdir's. The project lives under the GitHub account `rrgmc`, at `github.com/rrgmc/mirante`. No release has been published with the first identifier.

## Decision

- The bundle identifier is `com.rrgmc.mirante`, after the account that holds the project.
- The rest of decision 0101 stands.

## Alternatives

- **Keep `com.rangelreale.mirante`.** It matches favdir, but not the project's home.

## Consequences

- A Mac that ran a build with the first identifier asks once more for keychain access to each token.
- The identifier must not change again once a release is published, because the keychain and macOS's privacy settings know the app by it.
