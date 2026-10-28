#!/usr/bin/env python3
"""Checks that modules include only their own files and the module API.

Scans every .h and .cpp file under src/modules/. An #include fails the check
if its path starts with "../" or contains "/../", or if it names a core
header, that is, a path that starts with one of the core source folders.
<mirante/...> includes are the module API and are allowed.

Module tests in src/modules/<id>/tests/ are skipped. They register the module
into the real ItemTypeRegistry and link mirante_items, so they may include
core headers.

The build already keeps the core folders off a module's include path. This
check catches relative paths, which the build cannot block.

Prints each failure as file:line and exits with status 1 if there is any.
"""

import pathlib
import re
import sys

CORE_FOLDERS = ("core/", "items/", "layout/", "ui/", "platform/", "web/", "api/")
SOURCE_SUFFIXES = {".h", ".cpp"}
INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]')


def problem(path):
    """Returns why an include path is not allowed, or None if it is."""
    if path.startswith("../") or "/../" in path:
        return "path with \"..\""
    if path.startswith(CORE_FOLDERS):
        return "core header"
    return None


def main():
    repo = pathlib.Path(__file__).resolve().parent.parent
    modules = repo / "src" / "modules"
    failures = []
    files = sorted(p for p in modules.rglob("*") if p.is_file() and p.suffix in SOURCE_SUFFIXES
                   and p.relative_to(modules).parts[1:2] != ("tests",))
    for file in files:
        text = file.read_text(encoding="utf-8", errors="replace")
        for number, line in enumerate(text.splitlines(), start=1):
            match = INCLUDE.match(line)
            if not match:
                continue
            path = match.group(1).replace("\\", "/")
            reason = problem(path)
            if reason:
                name = file.relative_to(repo).as_posix()
                failures.append(f"{name}:{number}: {reason}: {line.strip()}")

    for failure in failures:
        print(failure)
    if failures:
        print(f"{len(failures)} include(s) not allowed in src/modules/. "
              "Modules may include their own files and <mirante/...> only.", file=sys.stderr)
        return 1
    print(f"Checked {len(files)} file(s) under src/modules/: no forbidden includes.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
