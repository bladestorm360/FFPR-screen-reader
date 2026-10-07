"""Offline name check: every game-code name the native mod uses, against each game's dump.cs.

The mod resolves classes, fields and methods by NAME at startup (src/core/refs.h). This tool
reads every declaration of the form

    ref::Class  kX{"Namespace", "Name"[, game::kFF1 | game::kFF2 ...][, "Assembly.dll"]};
    ref::Field  fY{kX, "field"[, "alt"]};
    ref::Method mZ{kX, "Method", argc};

(also without the `ref::` prefix inside `namespace ref` users, and the `TypedMethod` helpers are
ignored), works out the game mask of each class, and checks that every name exists in the
dump.cs of every game in that mask. Fields and methods are searched up the class hierarchy,
as il2cpp does at runtime.

Usage:  python tools/name_check.py [--games ff1,ff2] [--verbose]
Exit 1 when anything is missing. Report: build_check/name_report.txt
"""
import argparse
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FFPR = os.path.dirname(ROOT)
DUMPS = {
    "ff1": os.path.join(FFPR, "ff1", "dump.cs"),
    "ff2": os.path.join(FFPR, "ff2", "dump.cs"),
    "ff3": os.path.join(FFPR, "ff3", "dump.cs"),
    "ff4": os.path.join(FFPR, "FF4", "dump.cs"),
    "ff5": os.path.join(FFPR, "FF5", "dump.cs"),
    "ff6": os.path.join(FFPR, "ff6", "dump.cs"),
}
GAME_BITS = {"kFF1": "ff1", "kFF2": "ff2", "kFF3": "ff3", "kFF4": "ff4", "kFF5": "ff5", "kFF6": "ff6"}

CLASS_RE = re.compile(r'^(?:\[.*?\]\s*)?(?:(?:public|private|protected|internal|static|sealed|abstract|readonly|ref|unsafe|partial)\s+)*'
                      r'(class|struct|interface|enum)\s+([^\s:]+)(?:\s*:\s*([^/{]+?))?\s*(?://.*)?$')
METHOD_RE = re.compile(r'^\t(?:\[.*?\]\s*)?(?:(?:public|private|protected|internal|static|virtual|override|abstract|sealed|extern|new|unsafe|async)\s+)+'
                       r'.*?\s([\w<>.|`$]+)\((.*)\)\s*(?:\{\s*\}|;)')
FIELD_RE = re.compile(r'^\t(?:\[.*?\]\s*)?(?:(?:public|private|protected|internal|static|readonly|const|volatile|new)\s+)+'
                      r'[^()]*?\s([\w<>.`$|]+)(?:\s*=\s*[^;]*)?;')


def split_params(s):
    s = s.strip()
    if not s:
        return 0
    depth = 0
    count = 1
    for ch in s:
        if ch in "<([":
            depth += 1
        elif ch in ">)]":
            depth -= 1
        elif ch == "," and depth == 0:
            count += 1
    return count


def strip_generic(name):
    return re.sub(r"<[^<>]*>", "", name) if "<" in name and not name.startswith("<") else name


class Dump:
    def __init__(self, path):
        self.classes = {}      # (ns, name) -> dict(parent, fields:set, methods:{name:set(argc)})
        self.by_name = {}      # name -> [(ns, name)]
        ns = ""
        cur = None
        with open(path, encoding="utf-8", errors="replace") as f:
            for line in f:
                if line.startswith("// Namespace:"):
                    ns = line.split(":", 1)[1].strip()
                    continue
                if not line.startswith("\t") and not line.startswith(" "):
                    m = CLASS_RE.match(line.rstrip("\n"))
                    if m:
                        raw = m.group(2)
                        name = strip_generic(raw)
                        parent = (m.group(3) or "").split(",")[0].strip()
                        parent = strip_generic(parent.split("//")[0].strip())
                        cur = {"parent": parent, "fields": set(), "methods": {}}
                        self.classes[(ns, name)] = cur
                        self.by_name.setdefault(name, []).append((ns, name))
                    continue
                if cur is None:
                    continue
                if "{ get;" in line or "{ set;" in line:
                    continue
                m = METHOD_RE.match(line)
                if m:
                    mname = strip_generic(m.group(1))
                    cur["methods"].setdefault(mname, set()).add(split_params(m.group(2)))
                    continue
                m = FIELD_RE.match(line)
                if m:
                    cur["fields"].add(m.group(1))

    def find(self, ns, name):
        return self.classes.get((ns, name))

    def resolve_parent(self, ns, cls):
        p = cls["parent"]
        if not p:
            return None, None
        short = p.split(".")[-1]
        if "." in p:
            pns = p.rsplit(".", 1)[0]
            c = self.classes.get((pns, short))
            if c:
                return pns, c
        c = self.classes.get((ns, short))
        if c:
            return ns, c
        for key in self.by_name.get(short, []):
            return key[0], self.classes[key]
        return None, None

    def has_field(self, ns, name, field):
        seen = 0
        cls = self.find(ns, name)
        while cls and seen < 32:
            if field in cls["fields"]:
                return True
            ns, cls = self.resolve_parent(ns, cls)
            seen += 1
        return False

    def has_method(self, ns, name, method, argc):
        seen = 0
        cls = self.find(ns, name)
        while cls and seen < 32:
            if argc in cls["methods"].get(method, ()):
                return True
            ns, cls = self.resolve_parent(ns, cls)
            seen += 1
        return False


DECL_CLASS = re.compile(r'(?:ref::)?Class\s+(\w+)\s*\{\s*"([^"]*)"\s*,\s*"([^"]*)"\s*(?:,\s*([^,}"]+))?')
DECL_FIELD = re.compile(r'(?:ref::)?Field\s+(\w+)\s*\{\s*(\w+)\s*,\s*"([^"]*)"\s*(?:,\s*"([^"]*)")?')
DECL_METHOD = re.compile(r'(?:ref::)?Method\s+(\w+)\s*\{\s*(\w+)\s*,\s*"([^"]*)"\s*,\s*(\d+)')


def mask_games(expr):
    if not expr or "kAll" in expr:
        return list(DUMPS)
    games = [g for k, g in GAME_BITS.items() if re.search(r"\b" + k + r"\b", expr)]
    return games or list(DUMPS)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--games", default="ff1,ff2,ff3,ff4,ff5,ff6")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()
    wanted = args.games.split(",")

    decls = []   # per file: classes, fields, methods
    for dp, _, fn in os.walk(os.path.join(ROOT, "src")):
        for f in fn:
            if not f.endswith((".cpp", ".h")):
                continue
            path = os.path.join(dp, f)
            text = open(path, encoding="utf-8", errors="replace").read()
            # Declarations inside comments (header examples) are not uses.
            text = "\n".join(l for l in text.split("\n") if not l.lstrip().startswith("//"))
            rel = os.path.relpath(path, ROOT)
            # Local mask constants: constexpr uint8_t kFF134 = game::kFF1 | game::kFF3 | game::kFF4;
            local_masks = {m.group(1): m.group(2) for m in re.finditer(
                r'constexpr\s+[\w:]+\s+(\w+)\s*=\s*([^;]*kFF[^;]*);', text)}
            classes = {}
            for m in DECL_CLASS.finditer(text):
                expr = m.group(4)
                if expr and expr.strip() in local_masks:
                    expr = local_masks[expr.strip()]
                classes[m.group(1)] = (m.group(2), m.group(3), mask_games(expr))
            fields = [(m.group(1), m.group(2), m.group(3), m.group(4)) for m in DECL_FIELD.finditer(text)]
            methods = [(m.group(1), m.group(2), m.group(3), int(m.group(4))) for m in DECL_METHOD.finditer(text)]
            if classes or fields or methods:
                decls.append((rel, classes, fields, methods))

    dumps = {}
    for g in wanted:
        print(f"reading {g} dump.cs ...", flush=True)
        dumps[g] = Dump(DUMPS[g])

    problems = []
    checked = 0
    for rel, classes, fields, methods in decls:
        for var, (ns, name, games) in classes.items():
            for g in games:
                if g not in dumps:
                    continue
                checked += 1
                outer = name.split(".")[0]
                if not dumps[g].find(ns, name) and not dumps[g].find(ns, outer):
                    problems.append(f"{g}: class {ns}.{name} missing ({rel}, {var})")
        for var, cvar, field, alt in fields:
            if cvar not in classes:
                continue
            ns, name, games = classes[cvar]
            for g in games:
                if g not in dumps or not dumps[g].find(ns, name):
                    continue
                checked += 1
                if not dumps[g].has_field(ns, name, field) and not (alt and dumps[g].has_field(ns, name, alt)):
                    problems.append(f"{g}: field {name}.{field} missing ({rel}, {var})")
        for var, cvar, method, argc in methods:
            if cvar not in classes:
                continue
            ns, name, games = classes[cvar]
            for g in games:
                if g not in dumps or not dumps[g].find(ns, name):
                    continue
                checked += 1
                if not dumps[g].has_method(ns, name, method, argc):
                    problems.append(f"{g}: method {name}.{method}({argc}) missing ({rel}, {var})")

    os.makedirs(os.path.join(ROOT, "build_check"), exist_ok=True)
    report = os.path.join(ROOT, "build_check", "name_report.txt")
    with open(report, "w", encoding="utf-8") as f:
        f.write(f"{checked} checks, {len(problems)} missing\n")
        for p in sorted(problems):
            f.write(p + "\n")
    print(f"name check: {checked} checks, {len(problems)} missing (report: build_check\\name_report.txt)")
    if args.verbose:
        for p in sorted(problems)[:200]:
            print("  " + p)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
