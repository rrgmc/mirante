#!/usr/bin/env python3
"""Checks that an installed Mirante holds what a package needs.

    python tools/check-package.py <install folder>

The folder is the prefix given to `cmake --install --component Mirante`. The
script finds the layout from what is there: a Mirante.app bundle on macOS,
bin/mirante.exe on Windows, or opt/mirante on Linux. It fails, naming each
missing file, if the app lacks its executable, Qt's Portuguese translation
where QLibraryInfo::TranslationsPath points, the WebP image plugin, the SVG
plugins for the icons, or Qt WebEngine's helper and resources. See docs/plans/m10-packaging.md.
"""

import sys
from pathlib import Path

LANGUAGES = ["pt_BR"]


def windows(root: Path) -> list[str]:
    return [
        "bin/mirante.exe",
        "bin/qt.conf",
        "bin/Qt6Core.dll",
        "bin/Qt6WebEngineCore.dll",
        "bin/QtWebEngineProcess.exe",
        "bin/vcruntime140.dll",
        "plugins/platforms/qwindows.dll",
        "plugins/imageformats/qwebp.dll",
        "plugins/imageformats/qsvg.dll",
        "plugins/iconengines/qsvgicon.dll",
        "resources/qtwebengine_resources.pak",
        "resources/icudtl.dat",
        "translations/qtwebengine_locales/en-US.pak",
        *[f"translations/qtbase_{language}.qm" for language in LANGUAGES],
        "LICENSE",
    ]


def macos(root: Path) -> list[str]:
    contents = "mirante.app/Contents"
    webengine = f"{contents}/Frameworks/QtWebEngineCore.framework/Versions/A"
    return [
        f"{contents}/MacOS/mirante",
        f"{contents}/Info.plist",
        f"{contents}/Resources/qt.conf",
        f"{contents}/Resources/mirante.icns",
        f"{contents}/PlugIns/platforms/libqcocoa.dylib",
        f"{contents}/PlugIns/imageformats/libqwebp.dylib",
        f"{contents}/PlugIns/imageformats/libqsvg.dylib",
        f"{contents}/PlugIns/iconengines/libqsvgicon.dylib",
        f"{webengine}/Helpers/QtWebEngineProcess.app/Contents/MacOS/QtWebEngineProcess",
        f"{webengine}/Resources/qtwebengine_resources.pak",
        *[f"{contents}/translations/qtbase_{language}.qm" for language in LANGUAGES],
        "LICENSE",
    ]


def linux(root: Path) -> list[str]:
    app = "opt/mirante"
    return [
        f"{app}/bin/mirante",
        f"{app}/bin/qt.conf",
        f"{app}/lib/libQt6Core.so.6",
        f"{app}/lib/libQt6WebEngineCore.so.6",
        f"{app}/libexec/QtWebEngineProcess",
        f"{app}/plugins/platforms/libqxcb.so",
        f"{app}/plugins/imageformats/libqwebp.so",
        f"{app}/plugins/imageformats/libqsvg.so",
        f"{app}/plugins/iconengines/libqsvgicon.so",
        f"{app}/resources/qtwebengine_resources.pak",
        f"{app}/translations/qtwebengine_locales/en-US.pak",
        *[f"{app}/translations/qtbase_{language}.qm" for language in LANGUAGES],
        f"{app}/LICENSE",
        "usr/bin/mirante",
        "usr/share/applications/mirante.desktop",
        "usr/share/icons/hicolor/scalable/apps/mirante.svg",
        "etc/apparmor.d/mirante",
    ]


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__.strip(), file=sys.stderr)
        return 2
    root = Path(sys.argv[1])
    if (root / "mirante.app").is_dir():
        layout, expected = "macOS", macos(root)
    elif (root / "bin/mirante.exe").is_file():
        layout, expected = "Windows", windows(root)
    elif (root / "opt/mirante").is_dir():
        layout, expected = "Linux", linux(root)
    else:
        print(f"{root} holds no installed Mirante", file=sys.stderr)
        return 1

    missing = [path for path in expected if not (root / path).exists()]
    for path in missing:
        print(f"missing: {path}", file=sys.stderr)
    if missing:
        print(f"The {layout} install in {root} lacks {len(missing)} files.", file=sys.stderr)
        return 1
    print(f"The {layout} install in {root} has all {len(expected)} files checked.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
