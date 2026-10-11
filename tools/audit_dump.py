"""Read matching IL2CPP metadata for the accessibility audit (offline, no game launch).

Usage: python tools/audit_dump.py ff2 ClassName [method ...]
Prints class fields and selected methods with native RVAs and shared-body counts.
"""
import collections
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]


def classes(game):
    path = ROOT / {'ff4': 'FF4', 'ff5': 'FF5'}.get(game, game) / 'dump.cs'
    source = path.read_text(encoding='utf-8-sig').splitlines()
    shared = collections.Counter(re.findall(r'RVA: (0x[0-9A-Fa-f]+)', '\n'.join(source)))
    ns, current, result = '', None, []
    rva = None
    for line in source:
        if line.startswith('// Namespace:'):
            ns = line.partition(':')[2].strip()
        match = re.match(r'[^\t].*?\b(class|struct|enum|interface)\s+([^\s:]+)', line)
        if match:
            current = dict(namespace=ns, name=match[2], fields=[], methods=[])
            result.append(current)
        if current is None:
            continue
        match = re.search(r'RVA: (0x[0-9A-Fa-f]+)', line)
        if match:
            rva = match[1]
        elif line.startswith('\t') and '(' in line and ('{ }' in line or line.endswith(';')):
            current['methods'].append(dict(signature=line.strip(), rva=rva,
                                           folded=shared[rva] if rva else 0))
            rva = None
        elif line.startswith('\t') and ';' in line and '(' not in line:
            current['fields'].append(line.strip())
    return result


if __name__ == '__main__':
    game, name, *methods = sys.argv[1:]
    for cls in classes(game):
        if cls['name'] != name:
            continue
        print(cls['namespace'] + '.' + name)
        if not methods:
            print('\n'.join(cls['fields']))
        for method in cls['methods']:
            if methods and not any(re.search(r'\b' + re.escape(m) + r'\(', method['signature']) for m in methods):
                continue
            print(f"{method['rva']} shared={method['folded']} {method['signature']}")
