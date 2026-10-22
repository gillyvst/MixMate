#!/usr/bin/env python3
"""Builds Resources/headphones.tsv from a checkout of AutoEQ's results folder.

Usage:
    git clone --depth 1 --filter=blob:none --sparse https://github.com/jaakkopasanen/AutoEq.git
    (cd AutoEq && git sparse-checkout set --no-cone '/results/**/*ParametricEQ.txt')
    python3 tools/build_headphone_db.py AutoEq/results Resources/headphones.tsv

Each headphone appears once, taken from the most trusted measurement source.
Output format, one headphone per line (tab separated):
    name  source  type  preamp  filters
where filters is a comma separated list of "T fc gain q" with T = L (low shelf),
P (peak) or H (high shelf).
"""
import re
import sys
from pathlib import Path

# Most trusted first. Anything not listed comes after these, alphabetically.
SOURCE_PRIORITY = [
    "oratory1990",
    "crinacle",
    "Rtings",
    "Innerfidelity",
    "Headphone.com Legacy",
    "Super Review",
    "Kuulokenurkka",
]

# Within crinacle and Rtings, prefer these rigs.
RIG_PRIORITY = [
    "GRAS 43AG-7 over-ear",
    "EARS + 711 over-ear",
    "711 in-ear",
    "Bruel & Kjaer 4620 in-ear",
    "Bruel & Kjaer 5128 over-ear",
    "Bruel & Kjaer 5128 in-ear",
    "HMS II.3 over-ear",
    "HMS II.3 in-ear",
    "HMS II.3 earbud",
]

# Unit-to-unit duplicates that only clutter the list.
SKIP_NAME = re.compile(r"\((sample|serial number|unit)\b[^)]*\)\s*$", re.IGNORECASE)

FILTER_RE = re.compile(
    r"Filter\s+\d+:\s+ON\s+(LSC|PK|HSC)\s+Fc\s+([\d.]+)\s+Hz\s+Gain\s+(-?[\d.]+)\s+dB\s+Q\s+([\d.]+)"
)
PREAMP_RE = re.compile(r"Preamp:\s+(-?[\d.]+)\s+dB")
TYPE_CODES = {"LSC": "L", "PK": "P", "HSC": "H"}


def source_rank(source, rig):
    s = SOURCE_PRIORITY.index(source) if source in SOURCE_PRIORITY else len(SOURCE_PRIORITY)
    r = RIG_PRIORITY.index(rig) if rig in RIG_PRIORITY else len(RIG_PRIORITY)
    return (s, r, source.lower(), rig.lower())


def headphone_type(rig):
    for t in ("over-ear", "in-ear", "earbud"):
        if rig.endswith(t):
            return t
    return "over-ear"


def main(results_dir, out_path):
    best = {}
    for path in Path(results_dir).glob("*/*/*/*ParametricEQ.txt"):
        name = path.parent.name
        rig = path.parent.parent.name
        source = path.parent.parent.parent.name
        if SKIP_NAME.search(name):
            continue
        rank = source_rank(source, rig)
        if name in best and best[name][0] <= rank:
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        pre = PREAMP_RE.search(text)
        filters = FILTER_RE.findall(text)
        if not pre or not filters:
            continue
        flt = ",".join(f"{TYPE_CODES[t]} {fc} {g} {q}" for t, fc, g, q in filters)
        best[name] = (rank, f"{name}\t{source}\t{headphone_type(rig)}\t{pre.group(1)}\t{flt}")

    lines = [best[n][1] for n in sorted(best, key=str.lower)]
    Path(out_path).write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"Wrote {len(lines)} headphones to {out_path}")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
