# 0064. Keychain entries start with "mirante/"

- Status: Accepted
- Date: 2026-10-30

## Context

Tokens are kept with qtkeychain under service `mirante` and key `<workspace id>/<profile id>/<module id>/<field key>` ([decision 0059](0059-logins-per-workspace.md)). On Windows, qtkeychain ignores the service. Credential Manager then lists an entry such as `5a3e9c1d-…/default/gitlab/token`, which does not say which program it belongs to.

## Decision

The keychain backend names each entry `mirante/` followed by the key, such as `mirante/5a3e9c1d-…/default/gitlab/token`, on every platform. The key that `SecretStore` and the rest of the app use does not change; only the backend adds the prefix.

## Consequences

- Credential Manager shows entries that name Mirante, and they sort together.
- On macOS and Linux the service already says `mirante`, so the prefix repeats it. One rule for all platforms keeps platform code out of the backend.
- Tokens stored before this change, during the M5 checks, are not found. They are entered again, and the old entries are removed by hand.
