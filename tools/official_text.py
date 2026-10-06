#!/usr/bin/env python3
"""Read a Pixel Remaster game's own message tables in all 12 languages.

Used to check mod_text overrides (status ailments, stats, commands, vehicles...)
against each game's official wording. Read-only on the game files.

The message bundle (message_assets_all_*.bundle) holds four tables, each in
12 languages: system (menu labels, commands, stats, status ailments, item,
spell and vehicle names), story_cha (speaker names), story_mes (dialogue) and
etc_text. Rows are "MSG_ID<TAB>text"; the same MSG_ID is the same message in
every language.

Usage:
  python tools/official_text.py dump <ff1..ff5|all> <outdir>
      writes <outdir>/<game>_messages.json:
      {table: {lang: {msg_id: text}}}, markup removed
  python tools/official_text.py find <ff1..ff5> <regex> [--lang en] [--table system]
      prints every row whose text in --lang matches, with all 12 languages
  python tools/official_text.py id <ff1..ff5> <msg_id> [--table system]
      prints one row in all 12 languages

Environment: FFPR_STEAM overrides the Steam "common" folder.
"""

import argparse
import glob
import json
import os
import re
import sys
from collections import defaultdict

LANGS = ["en", "ja", "fr", "it", "de", "es", "ko", "zht", "zhc", "ru", "th", "pt"]

STEAM = os.environ.get("FFPR_STEAM", r"D:\Games\SteamLibrary\steamapps\common")
INSTALL = {
    "ff1": ("FINAL FANTASY PR", "FINAL FANTASY_Data"),
    "ff2": ("FINAL FANTASY II PR", "FINAL FANTASY II_Data"),
    "ff3": ("FINAL FANTASY III PR", "FINAL FANTASY III_Data"),
    "ff4": ("FINAL FANTASY IV PR", "FINAL FANTASY IV_Data"),
    "ff5": ("FINAL FANTASY V PR", "FINAL FANTASY V_Data"),
}

_MARKUP = re.compile(r"<[^>]*>")
_NAME = re.compile(r"^(.*)_(" + "|".join(LANGS) + r")$")


def aa_path(game):
    root, data = INSTALL[game]
    return os.path.join(STEAM, root, data, "StreamingAssets", "aa", "StandaloneWindows64")


def clean(text):
    return _MARKUP.sub("", text).replace("\\n", " ").strip()


def text_of(asset):
    raw = getattr(asset, "m_Script", b"")
    if isinstance(raw, str):
        raw = raw.encode("utf-8", "surrogateescape")
    return bytes(raw).decode("utf-8", "replace").lstrip("\ufeff")


def load(game):
    """{table: {lang: {msg_id: text}}} from the game's message bundle."""
    import UnityPy  # only needed when reading the game

    bundles = glob.glob(os.path.join(aa_path(game), "message_assets_all_*.bundle"))
    if not bundles:
        raise SystemExit(f"{game}: no message bundle under {aa_path(game)}")
    tables = defaultdict(dict)
    for obj in UnityPy.load(bundles[0]).objects:
        if obj.type.name != "TextAsset":
            continue
        asset = obj.read()
        m = _NAME.match(asset.m_Name)
        if not m:
            continue
        rows = {}
        for line in text_of(asset).splitlines():
            if "\t" in line:
                k, v = line.split("\t", 1)
                rows[k.strip()] = clean(v)
        tables[m.group(1)][m.group(2)] = rows
    return dict(tables)


def show(table, tables, mid):
    print(f"[{table}] {mid}")
    for lang in LANGS:
        print(f"  {lang}: {tables[table].get(lang, {}).get(mid, '')}")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    d = sub.add_parser("dump")
    d.add_argument("game")
    d.add_argument("outdir")
    f = sub.add_parser("find")
    f.add_argument("game")
    f.add_argument("regex")
    f.add_argument("--lang", default="en")
    f.add_argument("--table", default=None)
    i = sub.add_parser("id")
    i.add_argument("game")
    i.add_argument("msg_id")
    i.add_argument("--table", default="system")
    args = ap.parse_args()

    if args.cmd == "dump":
        games = list(INSTALL) if args.game == "all" else [args.game.lower()]
        os.makedirs(args.outdir, exist_ok=True)
        for g in games:
            tables = load(g)
            out = os.path.join(args.outdir, f"{g}_messages.json")
            with open(out, "w", encoding="utf-8") as fh:
                json.dump(tables, fh, ensure_ascii=False, indent=1)
            sizes = ", ".join(f"{t} {len(v.get('en', {}))}" for t, v in sorted(tables.items()))
            print(f"{g}: {sizes} -> {out}")
        return 0

    tables = load(args.game.lower())
    if args.cmd == "id":
        show(args.table, tables, args.msg_id)
        return 0
    rx = re.compile(args.regex)
    names = [args.table] if args.table else sorted(tables)
    hits = 0
    for t in names:
        for mid, text in tables[t].get(args.lang, {}).items():
            if rx.search(text):
                show(t, tables, mid)
                hits += 1
    print(f"{hits} rows")
    return 0


if __name__ == "__main__":
    sys.exit(main())
