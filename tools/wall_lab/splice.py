#!/usr/bin/env python3
"""Inline the tilesheet PNGs into scenarios_template.html -> scenarios.html.

The lab is published as a single self-contained artifact (a strict CSP blocks
every external request), so the sheets ship as base64 data: URIs rather than
files. This swaps each %%KEY%% placeholder for the matching blob.

    python tools/wall_lab/splice.py [-o OUT]

The rule itself lives in the template and needs none of this - tests/js stubs
Image out entirely and reads the template directly, so a sheet change can
never break the gate.
"""
import argparse
import pathlib
import re
import sys

HERE = pathlib.Path(__file__).parent
KEYS = ["FLOOR", "DECOR", "RAILS", "PROBS", "GROUND", "ROCKS", "ITEMS", "BUSH"]


def clean(b64: str) -> str:
    """Strip any data: prefix and all whitespace to leave a bare base64 payload."""
    return re.sub(r"\s+", "", re.sub(r"^data:image/\w+;base64,", "", b64.strip()))


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("-o", "--out", type=pathlib.Path, default=HERE / "scenarios.html")
    args = ap.parse_args()

    out = (HERE / "scenarios_template.html").read_text(encoding="utf-8")
    out = out.replace("%%WALLS%%", clean((HERE / "dwalls_b64.txt").read_text()))

    # sheets_b64.txt is TAB-separated "KEY<TAB>base64" lines.
    keyed = {}
    for line in (HERE / "sheets_b64.txt").read_text().splitlines():
        if "\t" in line:
            k, v = line.split("\t", 1)
            keyed[k.strip()] = clean(v)
    missing = [k for k in KEYS if k not in keyed]
    if missing:
        print(f"sheets_b64.txt is missing: {', '.join(missing)}", file=sys.stderr)
        return 1
    for k in KEYS:
        out = out.replace("%%" + k + "%%", keyed[k])

    left = re.findall(r"%%\w+%%", out)
    if left:
        print(f"unsubstituted placeholders remain: {sorted(set(left))}", file=sys.stderr)
        return 1

    args.out.write_text(out, encoding="utf-8")
    print(f"wrote {args.out} ({len(out):,} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
