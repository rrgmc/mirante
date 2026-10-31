#!/bin/sh
# Builds, tests, signs and notarises the macOS disk image on this Mac, then
# staples the notarisation ticket to it. With --upload, it also attaches the
# disk image to the GitHub release v<version>. The release workflow runs it
# too. See docs/releasing.md and docs/decisions/0101-macos-release-signing.md.
#
#   sh tools/release-macos.sh <version> [--upload]
#
# Needs a "Developer ID Application" certificate in the keychain, and
# notarisation credentials stored once with:
#
#   xcrun notarytool store-credentials mirante --apple-id <Apple ID> --team-id <team ID>
#
# MIRANTE_CODESIGN_IDENTITY overrides the certificate (default: "Developer ID
# Application") and MIRANTE_NOTARY_PROFILE the credentials profile (default:
# mirante). The release workflow uses an App Store Connect API key instead:
# MIRANTE_NOTARY_KEY (the path of the .p8 file), MIRANTE_NOTARY_KEY_ID and
# MIRANTE_NOTARY_ISSUER.

set -eu

usage() {
    echo "usage: sh tools/release-macos.sh <version> [--upload]" >&2
    exit 2
}

[ $# -ge 1 ] || usage
VERSION=${1#v}
UPLOAD=0
if [ $# -ge 2 ]; then
    [ "$2" = "--upload" ] || usage
    UPLOAD=1
fi
if ! echo "$VERSION" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+$'; then
    echo "Version '$1' is not X.Y.Z or vX.Y.Z" >&2
    exit 2
fi

IDENTITY=${MIRANTE_CODESIGN_IDENTITY:-Developer ID Application}
PROFILE=${MIRANTE_NOTARY_PROFILE:-mirante}
ROOT=$(cd "$(dirname "$0")/.." && pwd)
BUILD="$ROOT/build/macos-release"
DMG="$BUILD/mirante-$VERSION-macos.dmg"

# notarytool with the API key when one is set, otherwise with the stored profile.
notary() {
    if [ -n "${MIRANTE_NOTARY_KEY:-}" ]; then
        xcrun notarytool "$@" --key "$MIRANTE_NOTARY_KEY" \
            --key-id "$MIRANTE_NOTARY_KEY_ID" --issuer "$MIRANTE_NOTARY_ISSUER"
    else
        xcrun notarytool "$@" --keychain-profile "$PROFILE"
    fi
}

# Check the certificate and the credentials before the build, not after it.
if ! security find-identity -v -p codesigning | grep -q "\"$IDENTITY"; then
    echo "No codesigning identity matching '$IDENTITY' in the keychain." >&2
    exit 1
fi
if ! CHECK=$(notary history 2>&1); then
    echo "$CHECK" >&2
    echo "The notarytool credentials do not work. For a profile, store them with:" >&2
    echo "  xcrun notarytool store-credentials $PROFILE --apple-id <Apple ID> --team-id <team ID>" >&2
    exit 1
fi

QT_DIR=$(sh "$ROOT/tools/setup-qt.sh")

# A separate build folder, so the Developer ID setting does not reach everyday
# builds. Universal: Qt's libraries already are.
cmake --preset ci -B "$BUILD" -DCMAKE_PREFIX_PATH="$QT_DIR" \
    "-DMIRANTE_VERSION=$VERSION" "-DMIRANTE_MACOS_DISTRIBUTION_IDENTITY=$IDENTITY" \
    "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64"
cmake --build "$BUILD"
# Tests load data: URLs only, so Qt WebEngine's sandbox is not needed.
QTWEBENGINE_DISABLE_SANDBOX=1 ctest --test-dir "$BUILD" --output-on-failure

rm -rf "$BUILD/install"
cmake --install "$BUILD" --component Mirante --prefix "$BUILD/install"
python3 "$ROOT/tools/check-package.py" "$BUILD/install"

rm -f "$DMG"
(cd "$BUILD" && cpack)
codesign --sign "$IDENTITY" --timestamp "$DMG"

# notarytool can exit with 0 for a rejected submission, so check the status it prints.
RESULT=$(notary submit "$DMG" --wait)
echo "$RESULT"
if ! echo "$RESULT" | grep -q "status: Accepted"; then
    ID=$(echo "$RESULT" | awk '/^ *id:/ { print $2; exit }')
    echo "Notarisation failed. Apple's log:" >&2
    notary log "$ID" >&2 || true
    exit 1
fi
# Apple's ticket server sometimes drops the connection, so the staple is tried
# a few times before the release fails.
attempt=1
until xcrun stapler staple "$DMG"; do
    if [ "$attempt" -ge 5 ]; then
        echo "Stapling failed $attempt times." >&2
        exit 1
    fi
    attempt=$((attempt + 1))
    echo "Stapling failed; trying again in 30 seconds." >&2
    sleep 30
done
spctl --assess --type open --context context:primary-signature --verbose "$DMG"

echo "Notarised disk image: $DMG"

if [ "$UPLOAD" = 1 ]; then
    gh release upload "v$VERSION" "$DMG" --clobber
    echo "Attached to release v$VERSION"
fi
