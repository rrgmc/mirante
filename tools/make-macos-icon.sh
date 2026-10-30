#!/bin/sh
# Makes resources/app/mirante.icns, the macOS bundle's icon, from
# resources/icons/mirante.svg. Run it on macOS after changing the SVG, and
# commit the result. Requires rsvg-convert (brew install librsvg).
#
# macOS shows this file where the app is not running, such as in the Finder and
# in keychain prompts. While it runs, Qt sets the icon from the SVG.

set -eu

REPO=$(cd "$(dirname "$0")/.." && pwd)
SVG=$REPO/resources/icons/mirante.svg
OUT=$REPO/resources/app/mirante.icns

if ! command -v rsvg-convert >/dev/null 2>&1; then
    echo "rsvg-convert is required. Install it with: brew install librsvg" >&2
    exit 1
fi

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
ICONSET=$WORK/mirante.iconset
mkdir "$ICONSET"

for size in 16 32 128 256 512; do
    rsvg-convert -w "$size" -h "$size" "$SVG" -o "$ICONSET/icon_${size}x${size}.png"
    rsvg-convert -w $((size * 2)) -h $((size * 2)) "$SVG" -o "$ICONSET/icon_${size}x${size}@2x.png"
done

iconutil --convert icns --output "$OUT" "$ICONSET"
echo "Wrote $OUT"
