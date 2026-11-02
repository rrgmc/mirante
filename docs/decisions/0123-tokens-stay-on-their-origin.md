# 0123. A token goes only to the origin of its server

- Status: Accepted
- Date: 2026-11-02

## Context

[Issue 14](https://github.com/rrgmc/mirante/issues/14) asked whether `ServerClient` can send a module's token to another host. It can. A path that holds a full address resolves to that address. A test against two local servers showed that under Qt's default redirect policy, a custom header such as GitLab's `PRIVATE-TOKEN` follows a redirect to another host. [Decision 0121](0121-modules-are-trusted-code.md) requires that accidents like these cannot leak a token.

## Decision

- **The origin is the scheme, host and port of the base URL.**
- **A request whose resolved URL is on another origin is not sent.** It answers with a new error, `ApiReply::Error::OtherServer`. It does not mark the server as failing, because the mistake is the caller's.
- **`ServerClient` checks every redirect,** with `UserVerifiedRedirectPolicy`. A redirect on the same origin is followed. A redirect to another origin stops the request and answers `OtherServer`, with a message that names the address.
- **`http` to `https` on one host counts as another origin.** The user corrects the base URL, and the message says where the server points.

## Alternatives

- **Follow a redirect to another origin without the token.** Each such redirect would need its own request and handling, and without the token it would mostly fail. No module expects one. It can be added if a service needs it, such as one that sends files through other storage.
- **Allow `http` to `https` on one host.** It is friendlier, but it hides a wrong setting, and every later request starts over `http` with the token in clear.

## Consequences

- A server that moved to another address shows an error that names it, until the base URL changes.
- A module cannot use `ApiClient` to fetch from another origin. It uses its own request, which carries no token.
