#!/bin/sh
# Signs an installed Mirante.app. The install runs it after macdeployqt, which
# leaves the bundle's binaries unsigned. See
# docs/decisions/0100-macos-release-signing.md.
#
#   sh tools/sign-macos.sh <app bundle> <identity> <entitlements of QtWebEngineProcess>
#
# With the identity "-", it signs ad hoc, which is enough to start the app on
# the Mac that made it. With a certificate, it signs every piece from the
# inside out, with the hardened runtime and a secure timestamp, as
# notarisation requires, and gives QtWebEngineProcess its entitlements.

set -eu

[ $# -eq 3 ] || { echo "usage: sh tools/sign-macos.sh <app bundle> <identity> <entitlements>" >&2; exit 2; }
APP=$1
IDENTITY=$2
ENTITLEMENTS=$3

if [ "$IDENTITY" = "-" ]; then
    codesign --force --deep --sign - "$APP"
    codesign --verify --deep --strict "$APP"
    exit 0
fi

sign() {
    codesign --force --options runtime --timestamp --sign "$IDENTITY" "$@"
}

FRAMEWORKS="$APP/Contents/Frameworks"
HELPER="$FRAMEWORKS/QtWebEngineCore.framework/Versions/A/Helpers/QtWebEngineProcess.app"

# Loose libraries and plugins first.
find "$APP/Contents" -type f \( -name '*.dylib' -o -name '*.so' \) -print0 | xargs -0 -n 1 sh -c '
    codesign --force --options runtime --timestamp --sign "$0" "$1"' "$IDENTITY"

# The helper, with its entitlements, before the framework that holds it.
if [ -d "$HELPER" ]; then
    sign --entitlements "$ENTITLEMENTS" "$HELPER"
fi

# Each framework, after what it holds.
for framework in "$FRAMEWORKS"/*.framework; do
    [ -d "$framework" ] && sign "$framework"
done

# The app last.
sign "$APP"

codesign --verify --deep --strict --verbose=2 "$APP"
