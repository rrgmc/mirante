# 0091. The macOS build signs with the maintainer's certificate when the keychain has it

- Status: Accepted
- Date: 2026-10-30

## Context

[Decision 0090](0090-sign-the-macos-bundle.md) signs the bundle in the `bin` folder when `MIRANTE_MACOS_SIGN_IDENTITY` is set. The variable must be set before a build folder is first configured, so each new worktree and build folder needs it again. It also leaves the bundle that the linker makes ad hoc signed. A debugger starts that bundle, so macOS still asks for keychain access after each debug build.

## Decision

- `CMakeLists.txt` names the maintainer's Apple Development certificate. A certificate's name is not a secret.
- When `MIRANTE_MACOS_SIGN_IDENTITY` is empty and the keychain has that certificate, the build uses it. A certificate set in the variable still comes first.
- With a certificate, the build signs the bundle that the linker makes as well as the copy in `bin`. The first holds only the executable, so signing it is quick.

## Alternatives

- **Keep the variable only, set in the shell's profile.** This works, but each machine and shell needs the setting, and the debugged bundle stays ad hoc.
- **A preset for the maintainer's Mac.** Presets name a build folder, so this would need a second set of presets for the same builds.
- **Always sign with the certificate.** Signing fails where the keychain does not have it, which would break the build on other Macs.

## Consequences

- On the maintainer's Mac, every build folder and worktree is signed without any setting, and the keychain asks once per token, not after each build.
- Other Macs and CI build as before, signed ad hoc, unless they set the variable.
- The configure step runs `security find-identity` once on macOS.
