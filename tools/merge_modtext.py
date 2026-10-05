#!/usr/bin/env python3
"""Merge the five old mods' mod_text.json files into the shared layout.

Reads (never writes) the paused FF1-FF5 C# mod repositories and writes:

  data/common/mod_text.json   shared mod UI text, the union of all keys
  data/ffN/mod_text.json      per-game overrides (game terms only)
  docs/MODTEXT_MERGE.md       only the block between the GENERATED markers

Rules (see docs/MODTEXT_MERGE.md for the reasons):
  * A key in FF1 takes FF1's entry (parity rule: FF1 is the default).
  * A key not in FF1 takes the first of FF3, FF2, FF4, FF5 that has it.
  * A missing or empty language is filled from the other mods' entries
    for the same key, in the order FF1, FF3, FF2, FF4, FF5.
  * A game gets an override only for a game-term key whose entry in that
    game differs from the common entry. Wording drift gets no override.

Usage:  python tools/merge_modtext.py [--src-root DIR] [--check]
  --src-root  folder holding ff1/, ff2/, ff3/, FF4/, FF5/ (default: the
              parent of this repository)
  --check     verify the files already written; write nothing
Prints a short summary. Exit code 1 on any verification failure.
"""

import argparse
import json
import re
import sys
from pathlib import Path

LANGS = ["en", "ja", "fr", "it", "de", "es", "ko", "zht", "zhc", "ru", "th", "pt"]

# Source mod_text.json of each game, relative to --src-root.
SOURCES = {
    1: "ff1/ff1-screen-reader/mod_text.json",
    2: "ff2/ff2-screen-reader/mod_text.json",
    3: "ff3/ff3-screen-reader/mod_text.json",
    4: "FF4/ff4-screen-reader/mod_text.json",
    5: "FF5/ff5-screen-reader/mod_text.json",
}

# Key not in FF1: first game in this order wins (the plan's porting order).
NOT_IN_FF1_ORDER = [3, 2, 4, 5]
# Order used to fill a missing language.
FILL_ORDER = [1, 3, 2, 4, 5]

# Keys that ARE game terms: labels the game itself defines. Each game keeps
# its own entry when it differs from the common one.
_CATEGORIES = {
    "status ailment name": [
        "Paralysis", "Blind", "Stone", "Confusion", "KO", "Silence",
        "Sleep", "Poison", "Slow", "Stop", "Doom",
    ],
    "character stat label": [
        "Strength", "Agility", "Stamina", "Intellect", "Attack", "Accuracy",
        "Defense", "Evasion", "Vitality", "Evade", "Magic Defense",
        "Magic Evasion", "Spirit", "Next Level", "Next Level: {0}",
        "Strength: {0}", "Agility: {0}", "Stamina: {0}", "Intellect: {0}",
        "Accuracy: {0}%", "Defense: {0}", "Evasion: {0}%",
        "Strength +{0}", "Agility +{0}",
    ],
    "menu command in FF1/FF2, magic stat in FF4/FF5": ["Magic"],
    "job/class term": ["Job", "Job: {0}", "No unlocked jobs can equip"],
    "game menu command label": [
        "Item", "Equipment", "Status", "Sort", "Words", "Config",
        "Interruption", "Save", "Back", "Ability", "Load", "Buy", "Sell",
        "Use", "Key Items", "Equip", "Remove All", "Forget", "Remove",
        "Exchange", "Learn",
    ],
    "vehicle name": [
        "Ship", "Airship", "Canoe", "Chocobo", "On ship", "On canoe",
        "On airship", "ship", "airship", "Airship Landing",
        "Airship Landings",
    ],
    "equipment slot label": ["Head", "Body", "Accessory", "Accessory 2"],
    "weapon or armour type": ["Knife", "Staff", "magical armor"],
    "battle row label": ["Front Row", "Back Row"],
}
GAME_TERM_KEYS = {k: cat for cat, keys in _CATEGORIES.items() for k in keys}

# Keys whose English contains HP or MP: the abbreviation is a game term
# (FF1 uses LP/VIT/OZ in de/es/ru, FF2-FF4 KP/PV/HP, FF5 HP/MP). Such a key
# gets an override only when the game's abbreviation differs from the
# common entry's in some language. Other wording differences are drift.
HPMP_EN = re.compile(r"\b(HP|MP)\b")
_LETTER = "A-Za-zÀ-ɏЀ-ӿ"
HPMP_TOKEN = re.compile(
    rf"(?<![{_LETTER}])(HP|PV|KP|LP|VIT|ОЗ|MP|PM)(?![{_LETTER}])")
PLACEHOLDER = re.compile(r"\{\d+[^}]*\}")

BEGIN = "<!-- BEGIN GENERATED: tools/merge_modtext.py -->"
END = "<!-- END GENERATED -->"


def load_json(path):
    """Load a JSON object, rejecting duplicate keys."""
    def no_dups(pairs):
        seen = set()
        for k, _ in pairs:
            if k in seen:
                raise ValueError(f"{path}: duplicate key {k!r}")
            seen.add(k)
        return dict(pairs)
    return json.loads(path.read_bytes().decode("utf-8-sig"),
                      object_pairs_hook=no_dups)


def write_json(path, obj):
    path.parent.mkdir(parents=True, exist_ok=True)
    text = json.dumps(obj, ensure_ascii=False, indent=2) + "\n"
    path.write_bytes(text.encode("utf-8"))  # UTF-8, no BOM, LF


def hpmp_tokens(text):
    return sorted(set(HPMP_TOKEN.findall(text)))


def langs_differing(a, b):
    return [l for l in LANGS if a.get(l, "") != b.get(l, "")]


def merge(src):
    common, source_of, fills = {}, {}, []
    order = list(src[1]) + [k for g in NOT_IN_FF1_ORDER for k in src[g]]
    for key in order:
        if key in common:
            continue
        game = 1 if key in src[1] else next(
            g for g in NOT_IN_FF1_ORDER if key in src[g])
        entry = {l: src[game][key].get(l, "") for l in LANGS}
        for l in LANGS:
            if entry[l]:
                continue
            donor = next((g for g in FILL_ORDER
                          if g != game and src[g].get(key, {}).get(l)), None)
            if donor:
                entry[l] = src[donor][key][l]
                fills.append((key, l, donor))
        common[key] = entry
        source_of[key] = game

    overrides = {g: {} for g in SOURCES}
    reasons = {g: [] for g in SOURCES}
    for g in SOURCES:
        for key, own in src[g].items():
            base = common[key]
            diff = langs_differing(own, base)
            if not diff:
                continue
            reason = None
            if key in GAME_TERM_KEYS:
                reason = GAME_TERM_KEYS[key]
                if own.get("en") != base["en"]:
                    reason += f'; English "{own.get("en")}" (common "{base["en"]}")'
                else:
                    reason += "; differs in " + ", ".join(diff)
            elif HPMP_EN.search(base["en"]):
                tok = [l for l in LANGS
                       if hpmp_tokens(own.get(l, "")) != hpmp_tokens(base[l])]
                if tok:
                    shown = ", ".join(
                        f"{l} {'/'.join(hpmp_tokens(own[l])) or '-'}"
                        f" vs {'/'.join(hpmp_tokens(base[l])) or '-'}"
                        for l in tok)
                    reason = "HP/MP abbreviation: " + shown
            if reason:
                overrides[g][key] = {l: own.get(l, "") for l in LANGS}
                reasons[g].append((key, reason))
    return common, source_of, fills, overrides, reasons


def verify(common, overrides):
    """Return (gaps, placeholder_mismatches). Each a list of strings."""
    gaps, ph = [], []
    files = [("common", common)] + [(f"ff{g}", o) for g, o in overrides.items()]
    for name, data in files:
        for key, entry in data.items():
            for l in LANGS:
                if not isinstance(entry.get(l), str) or not entry[l]:
                    gaps.append(f"{name}: `{key}` lacks {l}")
            extra = set(entry) - set(LANGS)
            if extra:
                gaps.append(f"{name}: `{key}` has unknown languages {sorted(extra)}")
            want = sorted(PLACEHOLDER.findall(entry.get("en", "")))
            for l in LANGS[1:]:
                got = sorted(PLACEHOLDER.findall(entry.get(l, "")))
                if got != want:
                    ph.append(f"{name}: `{key}` {l} has {got or 'none'}, en has {want or 'none'}")
    return gaps, ph


def md_key(key):
    return "`" + key.replace("`", "ˋ") + "`"


def generated_block(src, common, source_of, fills, overrides, reasons, gaps, ph):
    out = [BEGIN, "", "## Generated report", "",
           "Written by `tools/merge_modtext.py`. Do not edit by hand.", ""]
    counts = {g: len(src[g]) for g in SOURCES}
    out.append("### Counts")
    out.append("")
    out.append("- Source keys: " + ", ".join(f"FF{g} {counts[g]}" for g in SOURCES) + ".")
    out.append(f"- Keys in the common file: {len(common)}.")
    by_src = {g: sum(1 for s in source_of.values() if s == g) for g in SOURCES}
    out.append("- Common entries taken from: " +
               ", ".join(f"FF{g} {by_src[g]}" for g in [1] + NOT_IN_FF1_ORDER) + ".")
    out.append("- Overrides: " +
               ", ".join(f"FF{g} {len(overrides[g])}" for g in SOURCES) + ".")
    out.append(f"- Languages filled from another mod: {len(fills)}.")
    out.append(f"- Missing-language gaps after the merge: {len(gaps)}.")
    out.append(f"- Placeholder mismatches (reported, not fixed): {len(ph)}.")
    out.append("")

    out.append("### Overrides")
    out.append("")
    for g in SOURCES:
        out.append(f"#### FF{g}: {len(reasons[g])}")
        out.append("")
        if not reasons[g]:
            out.append("None.")
        for key, why in reasons[g]:
            out.append(f"- {md_key(key)}: {why}.")
        out.append("")

    out.append("### Language fills")
    out.append("")
    if not fills:
        out.append("None. Every chosen entry already had all 12 languages.")
    for key, l, donor in fills:
        out.append(f"- {md_key(key)}: {l} taken from FF{donor}.")
    out.append("")

    out.append("### Gaps")
    out.append("")
    out.append("None." if not gaps else "")
    out.extend(f"- {x}" for x in gaps)
    out.append("")

    out.append("### Placeholder mismatches")
    out.append("")
    out.append("A translation whose `{n}` placeholders differ from its English. "
               "Reported only; the text is unchanged.")
    out.append("")
    out.append("None." if not ph else "")
    out.extend(f"- {x}" for x in ph)
    out.append("")

    out.append("### Keys not in FF1, by source")
    out.append("")
    out.append("The common entry for these came from the game named. "
               "Order tried: FF3, FF2, FF4, FF5.")
    out.append("")
    for g in NOT_IN_FF1_ORDER:
        keys = [k for k, s in source_of.items() if s == g]
        out.append(f"#### From FF{g}: {len(keys)}")
        out.append("")
        for k in keys:
            also = [f"FF{h}" for h in NOT_IN_FF1_ORDER if h != g and k in src[h]]
            out.append(f"- {md_key(k)}" + (f" (also in {', '.join(also)})" if also else ""))
        out.append("")
    out.append(END)
    return "\n".join(out)


def update_doc(path, block):
    text = path.read_bytes().decode("utf-8")
    if BEGIN not in text or END not in text:
        raise SystemExit(f"{path}: generated markers missing")
    head = text[:text.index(BEGIN)]
    tail = text[text.index(END) + len(END):]
    path.write_bytes((head + block + tail).encode("utf-8"))


def main():
    repo = Path(__file__).resolve().parent.parent
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--src-root", type=Path, default=repo.parent)
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()

    src = {g: load_json(args.src_root / rel) for g, rel in SOURCES.items()}
    union = set().union(*src.values())
    unknown = sorted(set(GAME_TERM_KEYS) - union)
    if unknown:
        raise SystemExit(f"game-term keys not in any source: {unknown}")

    common, source_of, fills, overrides, reasons = merge(src)
    data = repo / "data"

    if args.check:
        ok = load_json(data / "common" / "mod_text.json") == common and all(
            load_json(data / f"ff{g}" / "mod_text.json") == overrides[g]
            for g in SOURCES)
        if not ok:
            print("Out of date: run tools/merge_modtext.py")
            return 1
    else:
        write_json(data / "common" / "mod_text.json", common)
        for g in SOURCES:
            write_json(data / f"ff{g}" / "mod_text.json", overrides[g])

    # Verify what is on disk, not what is in memory.
    disk_common = load_json(data / "common" / "mod_text.json")
    disk_over = {g: load_json(data / f"ff{g}" / "mod_text.json") for g in SOURCES}
    gaps, ph = verify(disk_common, disk_over)
    if not args.check:
        update_doc(repo / "docs" / "MODTEXT_MERGE.md",
                   generated_block(src, common, source_of, fills, overrides,
                                   reasons, gaps, ph))

    print(f"common {len(disk_common)} keys; overrides " +
          ", ".join(f"FF{g} {len(disk_over[g])}" for g in SOURCES) +
          f"; fills {len(fills)}; gaps {len(gaps)}; placeholder mismatches {len(ph)}")
    return 1 if gaps else 0


if __name__ == "__main__":
    sys.exit(main())
