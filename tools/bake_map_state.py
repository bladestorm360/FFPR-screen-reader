"""Extract navigation-only map-initializer decisions, never dialogue or outcome text.

Input: matching offline mapdump scripts/objects. Output: per-game mapstate.json.
The small program retains flag tests, local shadow writes and entity-group selection.
It cannot call native scripts or change a save. Unknown control flow fails closed.
"""
import csv
import json
import pathlib
import sys
import collections
import bake_maps

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


def entry_guard(script, game):
    """Only the native control-flow prefix before the first real action.

    Calls and every unmodelled command remain actionable. No dialogue, outcome,
    script writes or native execution enters this availability predicate.
    """
    ms=script.get('mnemonics') or []
    entry=next((p['EntryPoint'] for p in script['segments'] if p['Label']=='Main'),0)
    todo=[entry];nodes={};empty_exit=False
    while todo:
        pc=todo.pop()
        if pc in nodes:continue
        if pc<0 or pc>=len(ms) or len(nodes)>=128:return None
        m=ms[pc];op=m['mnemonic']
        if op in ('Nop','Wait'):
            nodes[pc]=[0];todo.append(pc+1)
        elif op in ('Exit','Return'):
            nodes[pc]=[6];empty_exit=True
        elif op=='Jump':
            nodes[pc]=instruction(m,game);todo.append(nodes[pc][1])
        elif op=='Branch':
            nodes[pc]=instruction(m,game);todo.extend([pc+1,nodes[pc][6]])
        else:nodes[pc]=[1,'@action']
    if not empty_exit or not any(row[0]==1 for row in nodes.values()) or not any(
            row[0]==2 and 0<=row[1]<=8 for row in nodes.values()):return None
    offsets={};code=[]
    for pc,row in sorted(nodes.items()):
        offsets[pc]=len(code);code.append(list(row))
        if row[0]==1:code.append([6])
    for pc,row in nodes.items():
        target=code[offsets[pc]]
        if row[0]==3:target[1]=offsets[row[1]]
        elif row[0]==2:target[6]=offsets[row[6]]
    return {'entry':offsets[entry],'code':code}


def build(game):
    scripts = {}
    source={}
    for line in (DUMP / f'{game}_scripts.jsonl').read_text(encoding='utf-8').splitlines():
        s = json.loads(line)
        source[s['submap'],s['name']]=s
        if s['name'].lower() == 'sc_' + s['submap'].lower():
            main = next((p['EntryPoint'] for p in s['segments'] if p['Label'] == 'Main'), 0)
            scripts[s['submap']] = {'entry': main, 'code': [instruction(m, game) for m in s.get('mnemonics') or []]}
    objects=list(map(json.loads,(DUMP / f'{game}_objects.jsonl').read_text(encoding='utf-8').splitlines()))
    by_sub=collections.defaultdict(list)
    for o in objects:by_sub[o['submap']].append(o)
    submaps=json.loads((DUMP/f'{game}_submaps.json').read_text(encoding='utf-8'))
    bounds={sm:bake_maps.map_bounds(info,by_sub[sm]) for sm,info in submaps.items()}
    with (DUMP/f'{game}_master/script.txt').open(encoding='utf-8-sig') as f:
        masters={int(r['id']):r['script_name'] for r in csv.DictReader(line for line in f if line.strip()) if r['id']}
    guards=0
    for o in objects:
        p=o.get('props') or {}
        if o['type']==7 and p.get('flag_id',0)>0:
            row=scripts.setdefault(o['submap'],{'entry':0,'code':[]})
            row.setdefault('treasures',[]).append([p.get('entity_id',0),o['group'],p['flag_id']])
        if o['type'] not in (2,5,6,17,21,28,30) or not p.get('script_id') or p.get('action_id')!=2 or p.get('message_key'):continue
        name=masters.get(p['script_id'],'');s=source.get((o['submap'],name))
        guard=entry_guard(s,game) if s else None
        if guard is None:continue
        region=bounds[o['submap']]
        guard.update(id=p.get('entity_id',0),group=o['group'],type=o['type'],raw=o['name'],
                     cell=[bake_maps.layer_index(o['layer']),o['tx']-region[0],bake_maps.tile_row(o)-region[1]],source=name)
        row=scripts.setdefault(o['submap'],{'entry':0,'code':[]})
        row.setdefault('events',[]).append(guard);guards+=1
    output = ROOT / 'data' / game / 'mapstate.json'
    output.write_text(json.dumps(scripts, ensure_ascii=False, separators=(',', ':')) + '\n', encoding='utf-8')
    print(f'{game}: {len(scripts)} map-state records; {guards} bounded native entry guards')


if __name__ == '__main__':
    for game in ([f'ff{i}' for i in range(1, 7)] if sys.argv[1:] == ['all'] else sys.argv[1:]):
        build(game)
