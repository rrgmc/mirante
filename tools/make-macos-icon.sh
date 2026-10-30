#!/bin/sh
# Makes resources/app/mirante.icns, the macOS bundle's icon, from
# resources/icons/mirante.svg. Run it on macOS after changing the SVG, and
# commit the result. Requires rsvg-convert (brew install librsvg).
#
# macOS shows this file where the app is not running, such as in the Finder and
# in keychain prompts. While it runs, Qt sets the icon from the SVG.
#
# The icon follows the macOS icon grid: the SVG's rounded square, 240 of its
# 256 units from 8 to 248, fills 824 of 1024 pixels, 100 pixels in from each
# edge, so it is the size of other apps' icons.

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

# The SVG's drawing, without its outer <svg> element, placed on the grid.
GRID=$WORK/grid.svg
{
    echo '<svg xmlns="http://www.w3.org/2000/svg" width="1024" height="1024" viewBox="0 0 1024 1024">'
    echo '<g transform="translate(72.5333 72.5333) scale(3.43333)">'
    sed -e '1d' -e '$d' "$SVG"
    echo '</g>'
    echo '</svg>'
} > "$GRID"

for size in 16 32 128 256 512; do
    rsvg-convert -w "$size" -h "$size" "$GRID" -o "$ICONSET/icon_${size}x${size}.png"
    rsvg-convert -w $((size * 2)) -h $((size * 2)) "$GRID" -o "$ICONSET/icon_${size}x${size}@2x.png"
done

iconutil --convert icns --output "$OUT" "$ICONSET"
echo "Wrote $OUT"
