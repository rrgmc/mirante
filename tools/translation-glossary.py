#!/usr/bin/env python3
"""Writes the translators' terms at the top of translations/mirante_pt_BR.ts.

The terms are the table under "Terms" in translations/README.md. lupdate drops
comments outside messages, so `task translations` runs this after it.
"""

import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
README = REPO / "translations" / "README.md"
TS = REPO / "translations" / "mirante_pt_BR.ts"
START = "<!-- Terms:"
TS_TAG = re.compile(r"^<TS[ >]", re.MULTILINE)


def terms():
    text = README.read_text(encoding="utf-8")
    section = text.split("## Terms", 1)[1]
    rows = []
    for line in section.splitlines():
        cells = [cell.strip() for cell in line.strip().strip("|").split("|")]
        if len(cells) != 2 or not line.startswith("|") or set(cells[0]) <= set("-"):
            continue
        if cells == ["English", "Português"]:
            continue
        rows.append(f"{cells[0]}: {cells[1]}".replace("--", "-"))
    return rows


def main():
    rows = terms()
    if not rows:
        print("No terms found in translations/README.md.")
        return 1
    comment = (START + " each glossary term of docs/README.md always takes the same Portuguese\n"
               "     word. The list comes from translations/README.md; edit it there.\n"
               + "".join(f"     {row}\n" for row in rows) + "-->\n")
    ts = TS.read_text(encoding="utf-8")
    start = ts.find(START)
    if start >= 0:
        ts = ts[:start] + ts[ts.index("-->\n", start) + 4:]
    match = TS_TAG.search(ts)
    if not match:
        print(f"{TS.relative_to(REPO)} has no <TS> element.")
        return 1
    ts = ts[:match.start()] + comment + ts[match.start():]
    TS.write_text(ts, encoding="utf-8", newline="\n")
    print(f"Wrote {len(rows)} terms at the top of {TS.relative_to(REPO)}.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
