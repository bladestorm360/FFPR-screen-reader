"""Wiring check: every named action and query that something uses is defined somewhere.

Areas talk through names (src/core/core.h): actions::define / actions::run and
queries::define / queries::is / queries::text. A typo or a missing port does not fail the
build - it only logs "not available" at runtime. This finds those offline.

Uses = literal arguments of actions::run / actions::defined / queries::is / queries::text /
queries::defined, plus every string literal shaped like "Class.Member" in src/core/input
(the binding tables). Definitions = literal arguments of actions::define /
queries::define / queries::define_text. Names built at runtime ("Settings.Changed." + key)
are not seen.

Usage: python tools/wiring_check.py   (exit 1 when a used name has no definition)
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "src")

USE = re.compile(r'(?:actions::(?:run|defined)|queries::(?:is|text|defined))\s*\(\s*"([^"]+)"')
DEF = re.compile(r'(?:actions::define|queries::define(?:_text)?)\s*\(\s*"([^"]+)"')
# Input's own wrappers: run_action("X") and the binding tables reg_*(key, mod, "X", "label").
WRAP_USE = re.compile(r'(?:run_action|run_if_defined|query_is|query_text)\s*\(\s*"([^"]+)"')
REG_USE = re.compile(r'reg_\w*\([^;"]*"([A-Z]\w*\.[\w.]+)"')
# Names defined from a table: {"X", ...} entries in a file that defines with a variable.
TABLE_DEF = re.compile(r'\{\s*"([A-Z]\w*\.[A-Z][\w.]*)"\s*,')
VAR_DEFINE = re.compile(r'(?:actions::define|queries::define(?:_text)?)\s*\(\s*[a-z_][\w.\->]*\s*,')


def main():
    used = {}
    defined = {}
    for dp, _, fn in os.walk(SRC):
        for f in fn:
            if not f.endswith((".cpp", ".h")):
                continue
            path = os.path.join(dp, f)
            rel = os.path.relpath(path, ROOT)
            text = open(path, encoding="utf-8", errors="replace").read()
            text = "\n".join(l for l in text.split("\n") if not l.lstrip().startswith("//"))
            for m in DEF.finditer(text):
                defined.setdefault(m.group(1), set()).add(rel)
            for m in USE.finditer(text):
                used.setdefault(m.group(1), set()).add(rel)
            for rx in (WRAP_USE, REG_USE):
                for m in rx.finditer(text):
                    used.setdefault(m.group(1), set()).add(rel)
            if VAR_DEFINE.search(text):
                for m in TABLE_DEF.finditer(text):
                    defined.setdefault(m.group(1), set()).add(rel)
    missing = sorted(n for n in used if n not in defined and not n.startswith("Settings.Changed."))
    unused = sorted(n for n in defined if n not in used)
    out = os.path.join(ROOT, "build_check", "wiring_report.txt")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "w", encoding="utf-8") as f:
        f.write(f"{len(used)} names used, {len(defined)} defined, {len(missing)} used but undefined\n\n")
        f.write("USED BUT NOT DEFINED\n")
        for n in missing:
            f.write(f"  {n}  <- {', '.join(sorted(used[n]))}\n")
        f.write("\nDEFINED BUT NOT USED (fine when called by a hook or kept for parity)\n")
        for n in unused:
            f.write(f"  {n}  ({', '.join(sorted(defined[n]))})\n")
    print(f"wiring: {len(used)} used, {len(defined)} defined, {len(missing)} undefined "
          f"(report: build_check\\wiring_report.txt)")
    for n in missing:
        print(f"  undefined: {n}")
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
