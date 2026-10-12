"""Extract serialized growth tables and verify their native population paths offline."""
from pathlib import Path
import json
import re
import UnityPy

ROOT = Path(__file__).resolve().parents[2]
STEAM = Path('D:/Games/SteamLibrary/steamapps/common')
GAMES = ['FINAL FANTASY PR', 'FINAL FANTASY II PR', 'FINAL FANTASY III PR',
         'FINAL FANTASY IV PR', 'FINAL FANTASY V PR', 'FINAL FANTASY VI PR']
records = []
for n, game in enumerate(GAMES, 1):
    source = (ROOT.parent / ('ff%d' % n) / 'dump.cs').read_text(encoding='utf-8-sig')
    block = source.split('public enum ParameterType //', 1)[1].split('\n}', 1)[0]
    names = {int(value): name for name, value in re.findall(
        r'public const ParameterType (\w+) = (-?\d+);', block)}
    folder = STEAM / game
    data = next(p for p in folder.iterdir() if p.name.endswith('_Data') and p.is_dir())
    paths = list((data / 'StreamingAssets/aa/StandaloneWindows64').glob('key_battle_assets_all_*.bundle'))
    assert len(paths) == 1
    env = UnityPy.load(str(paths[0]))
    trees, game_objects = {}, {}
    for obj in env.objects:
        if obj.type.name == 'GameObject':
            game_objects[obj.path_id] = obj.read().m_Name
        elif obj.type.name == 'MonoBehaviour':
            try:
                trees[obj.path_id] = obj.read_typetree()
            except Exception:
                pass
    # ResultStatusContentView/Controller have this distinctive before/after pair.
    views = {pid for pid, tree in trees.items() if 'categoryText' in tree and 'beforValueText' in tree and 'afterValueText' in tree}
    rows = {pid: tree for pid, tree in trees.items() if 'type' in tree
            and tree.get('view', {}).get('m_PathID') in views}
    tables = []
    for pid, tree in trees.items():
        content = tree.get('contentList', [])
        if not isinstance(content, list) or not content or not any(row.get('m_PathID') in rows for row in content):
            continue
        table = {'path_id': pid, 'object': game_objects.get(tree['m_GameObject']['m_PathID']), 'rows': [], 'other_rows': []}
        for pointer in content:
            if pointer['m_PathID'] not in rows:
                table['other_rows'].append(pointer['m_PathID'])
                continue
            row = rows[pointer['m_PathID']]
            view = trees[row['view']['m_PathID']]
            table['rows'].append({'type': names[row['type']], 'subtype': names[row.get('subType', 0)],
                                  'view_fields': sorted(k for k, v in view.items() if isinstance(v, dict) and v.get('m_PathID'))})
        tables.append(table)
    assert tables, 'FF%d growth table missing' % n
    edges = ROOT / ('docs/re/ff%d_results_edges.tsv' % n)
    native = edges.read_text(encoding='utf-8')
    ns = 'Serial.FF%d.UI.KeyInput' % (0 if n == 4 else n)
    assert any(line.startswith(ns + '.ResultStatusUpController.SetData\t') and
               'Last.UI.KeyInput.ResultStatusContentController.SetData' in line for line in native.splitlines())
    types = sorted({row['type'] for table in tables for row in table['rows']})
    records.append({'game': n, 'bundle': paths[0].name, 'tables': tables, 'native_edges': edges.name})
    print('FF%d: displayed growth rows: %s; native before/after producer verified' % (n, ', '.join(types)))
(ROOT / 'docs/re/results_growth_assets.json').write_text(json.dumps(records, indent=2) + '\n', encoding='utf-8')
