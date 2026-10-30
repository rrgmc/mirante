# 0090. Sign the macOS bundle with a certificate when one is set

- Status: Accepted
- Date: 2026-10-30

## Context

macOS keeps a list of the apps that may read each keychain entry. **Always Allow** adds the running app to that list. A local build has only an ad hoc signature, which identifies the app by a hash of its binary. Each build therefore looks like a new app to the keychain, and macOS asks again for each token. A bundle changed by `macdeployqt` also fails strict signature verification.

## Decision

- The cache variable `MIRANTE_MACOS_SIGN_IDENTITY` names a code signing certificate. Its default comes from the environment variable of the same name.
- When it is set, `macdeployqt` signs the bundle in the `bin` folder with that certificate. The signature then identifies the app by its bundle identifier and certificate, which stay the same across builds.
- When it is empty, the build behaves as before. The build needs no certificate, so other machines and CI still work.

## Alternatives

- **Keep one secret entry for all tokens.** This reduces the number of prompts but still asks after each build, and it changes how tokens are stored.
- **Move tokens to a settings file.** This removes the prompts by giving up the keychain's protection.
- **A self-signed certificate.** This also gives a stable signature, but each developer has to create and trust one. It still works with this variable.

## Consequences

- After one **Always Allow** for each prompt, later builds signed with the same certificate read the tokens without asking.
- Each link signs the whole bundle, including Qt WebEngine, so deploying takes longer.
- The signature is for local use. It has no hardened runtime or notarization, so it does not prepare the app for distribution.
