# Releasing

A release is a GitHub release with three files: the Windows setup, the macOS disk image and the Ubuntu `.deb` package. The workflow `.github/workflows/release.yml` makes them from a version tag. See [the M10 plan](plans/m10-packaging.md) and decisions [0098](decisions/0098-packages-from-cmake-install.md), [0099](decisions/0099-linux-package-in-opt.md) and [0100](decisions/0100-macos-release-signing.md).

## Making a release

1. Check that CI is green on `master`.
2. Tag the commit and push the tag:

   ```sh
   git tag v1.2.0
   git push origin v1.2.0
   ```

3. The workflow publishes the release with notes generated from the commits, then builds, tests and attaches the three files. The macOS job may wait for an approval, if the `release` environment asks for one.
4. Raise `MIRANTE_VERSION` in `CMakeLists.txt` to the next version, so builds between releases do not report the released one.

The tag must be `vX.Y.Z`. The app's version is the tag without the `v`.

## Building a tag again

If a job failed, run the workflow by hand: Actions > Release > Run workflow, with the tag. The code comes from the tag, and the workflow file from the branch it runs on. The release is kept, and each file is replaced.

## The `release` environment

The macOS job runs in the GitHub environment `release`, which holds the signing secrets. Set it up once in the repository's Settings > Environments:

- **Deployment branches and tags:** selected only, with the tag rule `v*`.
- **Required reviewers:** optional. With one, every release waits for an approval before the secrets are used.
- **Secrets:**

  | Secret | Value |
  |---|---|
  | `MACOS_CERTIFICATE_P12` | The "Developer ID Application" certificate with its private key, exported from Keychain Access as `.p12`, in Base64: `base64 -i certificate.p12` |
  | `MACOS_CERTIFICATE_PASSWORD` | The password given to the `.p12` at export |
  | `NOTARY_API_KEY` | The contents of an App Store Connect API key file, `AuthKey_<id>.p8`, with the Developer role |
  | `NOTARY_API_KEY_ID` | That key's id |
  | `NOTARY_API_ISSUER` | The issuer id shown above the keys in App Store Connect |

favdir's `release` environment uses the same secrets, so they can be copied from there.

## The macOS disk image by hand

On a Mac with the Developer ID certificate in the keychain:

```sh
xcrun notarytool store-credentials mirante --apple-id <Apple ID> --team-id <team ID>   # once
sh tools/release-macos.sh 1.2.0            # builds, tests, signs, notarises and staples
sh tools/release-macos.sh 1.2.0 --upload   # and replaces the file on the release v1.2.0
```

The script uses its own build folder, `build/macos-release`.

## What is not signed

The Windows setup is not signed, so SmartScreen warns on its first start. The `.deb` package is not signed either; `apt` installs a local file without a signature. The packages that CI keeps for each push are unsigned on all three platforms, and the macOS one is signed ad hoc.
