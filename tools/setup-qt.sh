#!/bin/sh
# Downloads the prebuilt Qt that Mirante builds against, with aqtinstall.
# Requires uv (https://docs.astral.sh/uv/).
#
# Qt goes into the folder named by the environment variable MIRANTE_QT_ROOT,
# or into .qt/ in the repository if it is not set. Several projects can share
# one Qt by setting MIRANTE_QT_ROOT to the same folder.
#
# Prints the Qt folder to pass to CMake as CMAKE_PREFIX_PATH.

set -eu

QT_VERSION=6.8.3

case "$(uname -s)" in
    Darwin) QT_HOST=mac; QT_ARCH=clang_64; QT_ARCH_DIR=macos; QT_EXTRA_ARCHIVES= ;;
    # Qt for Linux ships its own ICU, which its libraries and tools need.
    Linux)  QT_HOST=linux; QT_ARCH=linux_gcc_64; QT_ARCH_DIR=gcc_64; QT_EXTRA_ARCHIVES=icu ;;
    *) echo "Unsupported system: $(uname -s). On Windows, use tools/setup-qt.ps1." >&2; exit 1 ;;
esac

REPO=$(cd "$(dirname "$0")/.." && pwd)
QT_ROOT=${MIRANTE_QT_ROOT:-$REPO/.qt}
QT_DIR=$QT_ROOT/$QT_VERSION/$QT_ARCH_DIR

# qtsvg provides the plugin that loads the SVG icons of item types.
if [ ! -d "$QT_DIR/lib/cmake/Qt6Svg" ]; then
    if ! command -v uvx >/dev/null 2>&1; then
        echo "uv is required to download Qt. Install it from https://docs.astral.sh/uv/ and run this script again." >&2
        exit 1
    fi
    echo "Installing Qt $QT_VERSION into $QT_ROOT" >&2
    uvx --from aqtinstall aqt install-qt "$QT_HOST" desktop "$QT_VERSION" "$QT_ARCH" -O "$QT_ROOT" --archives qtbase qtsvg $QT_EXTRA_ARCHIVES >&2
fi

echo "$QT_DIR"
