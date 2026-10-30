# 0063. Tokens follow a workspace id that another device changed

- Status: Accepted
- Date: 2026-10-29

## Context

[Decision 0062](0062-logins-follow-a-new-workspace-id.md) moves a workspace's logins when another device changed its id. It leaves tokens behind, because the keychain cannot list its keys, so the user enters every token again on that device.

Listing is not needed. A token's key is `<workspace id>/<profile id>/<module id>/<field key>`. The profiles are in `workspace.mirante`, and the field keys are the `Secret` fields of the modules' settings screens in this build. Every key a workspace can have is known.

## Decision

- **Tokens move with the logins.** When a workspace opens with an id that differs from the one this device remembers, at the same point where its logins move, the app builds every token key of the old id from the workspace's profiles and the build's `Secret` fields. For each token found under the old key, it writes the token under the new key, then removes the old one.
- **A token already under the new key wins.** It is left alone, and the old one is removed.
- **An id this device gives takes nothing**, as for logins.
- **Tokens of modules not in this build stay** under the old key, since their field keys are unknown. A build with the module moves them the next time the id changes, or never.
- The move is asynchronous, like every keychain call. Requests of the moved profiles wait until it is done, so they do not report "Token not set" in between.

This supersedes the part of decision 0062 that leaves tokens behind.

## Alternatives

- **Leave tokens behind,** as decision 0062 does. The user types every token again after a race that is not their doing.
- **Key tokens without the workspace id.** Tokens would survive any change of id, but two workspaces with a profile of the same id, such as `default`, would share tokens again, which decision 0059 ended.

## Consequences

- After another device changes the id, this device keeps its tokens as well as its logins.
- Opening such a workspace makes a few keychain calls more, once.
