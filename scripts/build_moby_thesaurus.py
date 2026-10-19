#!/usr/bin/env python3
"""Build a StarDict thesaurus for Jester from the Moby Thesaurus.

Input: words.txt from https://github.com/words/moby, the Moby Thesaurus II by
Grady Ward, which is in the public domain. One entry per line: the headword,
then its synonyms, separated by commas. Only words.txt is used; the moby npm
package's Open Office thesaurus data (a separate dependency) is not.

Output, in OUTPUT_DIR: <stem>.ifo, <stem>.idx and <stem>.dict with HTML
definitions (sametypesequence=h). Run `dictionary_tools.py prep OUTPUT_DIR`
afterwards to add the .idx.oft and .idx.oft.cspt lookup accelerators.

Each definition is "Synonyms (N)" followed by the synonyms, sorted
case-insensitively, empty items and repeats removed. Headwords that differ
only in case ("advent", "Advent") stay separate entries, since Jester prefers
the exact-case match; each lists the others under "See also" so every sense
can be reached.

The .idx is sorted by (lowercase, original), the order dictionary_tools.py
uses and the case-insensitive order Jester's lookup scans.

Usage: python3 scripts/build_moby_thesaurus.py words.txt OUTPUT_DIR [--stem NAME]
"""

import argparse
import html
import struct
import sys
from collections import OrderedDict
from datetime import date
from pathlib import Path


def read_entries(path: Path) -> "OrderedDict[str, list[str]]":
    text = path.read_bytes().decode("ascii")
    entries: "OrderedDict[str, list[str]]" = OrderedDict()
    for line in text.replace("\r\n", "\n").split("\n"):
        if not line.strip():
            continue
        head, *synonyms = line.split(",")
        head = head.strip()
        if not head:
            continue
        merged = entries.setdefault(head, [])
        merged.extend(s.strip() for s in synonyms)
    return entries


def clean(synonyms: list[str]) -> list[str]:
    seen = set()
    out = []
    for s in synonyms:
        if s and s not in seen:
            seen.add(s)
            out.append(s)
    return sorted(out, key=lambda s: (s.lower(), s))


def definition(synonyms: list[str], see_also: list[str]) -> bytes:
    parts = [f"<p><b>Synonyms</b> ({len(synonyms)})</p>",
             "<p>" + ", ".join(html.escape(s, quote=False) for s in synonyms) + "</p>"]
    if see_also:
        parts.append("<p><b>See also</b> " + ", ".join(html.escape(s, quote=False) for s in see_also) + "</p>")
    return "".join(parts).encode("utf-8")


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("words", type=Path, help="words.txt from github.com/words/moby")
    ap.add_argument("output", type=Path, help="Output folder")
    ap.add_argument("--stem", default="Moby-Thesaurus", help="File name stem (default Moby-Thesaurus)")
    args = ap.parse_args()

    entries = read_entries(args.words)
    by_folded: dict[str, list[str]] = {}
    for head in entries:
        by_folded.setdefault(head.lower(), []).append(head)

    heads = sorted(entries, key=lambda w: (w.lower(), w))
    args.output.mkdir(parents=True, exist_ok=True)
    dict_path = args.output / f"{args.stem}.dict"
    idx_path = args.output / f"{args.stem}.idx"
    ifo_path = args.output / f"{args.stem}.ifo"
    for p in (dict_path, idx_path, ifo_path):
        if p.exists():
            sys.exit(f"{p} exists; choose an empty output folder")

    idx = bytearray()
    offset = 0
    synonym_total = 0
    with dict_path.open("wb") as dict_file:
        for head in heads:
            synonyms = clean(entries[head])
            synonym_total += len(synonyms)
            see_also = [h for h in by_folded[head.lower()] if h != head]
            body = definition(synonyms, see_also)
            dict_file.write(body)
            idx += head.encode("utf-8") + b"\0" + struct.pack(">II", offset, len(body))
            offset += len(body)
    idx_path.write_bytes(bytes(idx))

    ifo = [
        "StarDict's dict ifo file",
        "version=2.4.2",
        "bookname=Moby Thesaurus",
        f"wordcount={len(heads)}",
        f"idxfilesize={len(idx)}",
        "sametypesequence=h",
        "author=Grady Ward",
        "website=https://github.com/words/moby",
        f"date={date.today().strftime('%Y.%m.%d')}",
        "description=Moby Thesaurus II by Grady Ward, public domain. "
        "From words.txt of github.com/words/moby, formatted for Jester.",
    ]
    ifo_path.write_text("\n".join(ifo) + "\n", encoding="utf-8")
    print(f"{len(heads)} headwords, {synonym_total} synonyms, .dict {offset} bytes, .idx {len(idx)} bytes")


if __name__ == "__main__":
    main()
