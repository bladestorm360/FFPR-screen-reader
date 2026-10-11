"""Extract navigation-only map-initializer decisions, never dialogue or outcome text.

Input: matching offline mapdump scripts/objects. Output: per-game mapstate.json.
The small program retains flag tests, local shadow writes and entity-group selection.
It cannot call native scripts or change a save. Unknown control flow fails closed.
"""
import csv
import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).absolute().parents[1]
DUMP = ROOT.parent / 'tools' / 'mapdump'
CATEGORIES = {**{f'ScenarioFlag{i+1}': i for i in range(4)},
              **{f'TreasureFlag{i+1}': i+4 for i in range(4)},
              'ScenarioVariable': 8, 'AreaLocalVariable': 9,
              'MapLocalVariable': 10, 'ScriptLocalVariable': 11,
              'MessageSelection': 12, 'ItemSelection': 13, 'imm': -1}
OPERATORS = {'＝': 0, '≠': 1, '＜': 2, '＞': 3, '≦': 4, '≧': 5, '←': 6}


def instruction(m, game):
    op = m['mnemonic']
    values = m.get('operands') or {}
    ints = list(values.get('iValues') or []) + [0] * 8
    strings = list(values.get('sValues') or []) + [''] * 8
    if op == 'SetEntities': return [1, strings[0]]
    if op == 'Branch':
        return [2, CATEGORIES.get(strings[0], -2), ints[0],
                OPERATORS.get(strings[1], -1), CATEGORIES.get(strings[2], -2), ints[1], ints[2]]
    if op == 'Jump': return [3, ints[0]]
    if op == 'Call': return [4, ints[0]]
    if op == 'Return': return [5]
    if op == 'Exit': return [6]
    if op in ('SetFlag', 'ResetFlag'):
        return [7, CATEGORIES.get(strings[0], -2), ints[0], int(op == 'SetFlag')]
    if op == 'Calc' and strings[1] == '←':
        return [8, CATEGORIES.get(strings[0], -2), ints[0], CATEGORIES.get(strings[2], -2), ints[1]]
    if op == 'SetRandomValue': return [8, CATEGORIES.get(strings[0], -2), ints[0], -2, 0]
    # FF1 Current.MapEntryRoofControl only calls IMapAccessor slots 10/20:
    # map-layer rendering and minimap-icon visibility. It never selects an entity
    # group or writes DataStorage. Matching native manifest: docs/re/ff1_roof*.
    if game == 'ff1' and op == 'SysCall' and strings[0] == 'MapEntryRoofControl': return [0]
    # These can change control flow/state beyond this read-only model.
    if op.startswith('Branch') or op in ('Calc', 'ChangeScript', 'SysCall', 'ChangeMap', 'Encount'):
        return [9]
    return [0]


def build(game):
    scripts = {}
    for line in (DUMP / f'{game}_scripts.jsonl').read_text(encoding='utf-8').splitlines():
        s = json.loads(line)
        if s['name'].lower() == 'sc_' + s['submap'].lower():
            main = next((p['EntryPoint'] for p in s['segments'] if p['Label'] == 'Main'), 0)
            scripts[s['submap']] = {'entry': main, 'code': [instruction(m, game) for m in s.get('mnemonics') or []]}
    for line in (DUMP / f'{game}_objects.jsonl').read_text(encoding='utf-8').splitlines():
        o = json.loads(line)
        if o['type'] != 7: continue
        flag = (o.get('props') or {}).get('flag_id', 0)
        if flag <= 0: continue
        row = scripts.setdefault(o['submap'], {'entry': 0, 'code': []})
        row.setdefault('treasures', []).append([o['props'].get('entity_id', 0), o['group'], flag])
    output = ROOT / 'data' / game / 'mapstate.json'
    output.write_text(json.dumps(scripts, ensure_ascii=False, separators=(',', ':')) + '\n', encoding='utf-8')
    print(f'{game}: {len(scripts)} map-state records')


if __name__ == '__main__':
    for game in ([f'ff{i}' for i in range(1, 7)] if sys.argv[1:] == ['all'] else sys.argv[1:]):
        build(game)
