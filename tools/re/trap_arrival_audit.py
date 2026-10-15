"""All-six offline environmental trap arrival survey; no capture or game launch.

Flag continuation scripts that reposition the player rather than silently treating
their ChangeMap PointIn as final. Raw script evidence stays local/ignored.
"""
from pathlib import Path
import json
import re
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import bake_maps as b

ROOT = Path(__file__).resolve().parents[2]
DUMP = ROOT.parent / 'tools/mapdump'
report = []
for n in range(1, 7):
    game = f'ff{n}'
    scripts = {(s['submap'], s['name']): s for s in map(json.loads, (DUMP / f'{game}_scripts.jsonl').read_text(encoding='utf-8').splitlines())}
    h, rows = b.read_csv(DUMP / f'{game}_master/script.txt')
    masters = {int(r[h.index('id')]): r[h.index('script_name')] for r in rows}
    h, rows = b.read_csv(DUMP / f'{game}_master/map.txt')
    maps = {int(r[h.index('id')]): r[h.index('map_name')] for r in rows}
    objects = list(map(json.loads, (DUMP / f'{game}_objects.jsonl').read_text(encoding='utf-8').splitlines()))
    candidates = 0
    position_changes = 0
    for o in objects:
        name = o.get('name', '')
        if o['type_name'] not in ('Event','RandomEvent') or not (any(k in name for k in ('落とし穴','落し穴','穴に落ちる','地割れに落ちる','pitfall','trapdoor')) or re.fullmatch(r'落ちる[0-9０-９]*', name)):
            continue
        candidates += 1
        script_name = masters.get(o['props'].get('script_id'), '')
        script = scripts.get((o['submap'], script_name), {})
        if not script:
            shared=[s for s in scripts.values() if s['map']==o['map'] and s['name']==script_name]
            if len(shared)==1:script=shared[0]
        changes = []
        main = next((s for s in script.get('segments', []) if s['Label']=='Main'), None)
        commands = script.get('mnemonics', [])
        if main:
            commands = commands[main['EntryPoint']:main['EntryPoint']+main['Count']]
        for m in commands:
            if m['mnemonic'] != 'ChangeMap': continue
            iv, sv = m['operands'].get('iValues', []), m['operands'].get('sValues', [])
            if len(iv)<2: continue
            cont_name=sv[0] if sv else ''
            cont=scripts.get((maps.get(iv[0]),cont_name),{})
            puppet_entries=[]
            for command in cont.get('mnemonics',[]):
                if command['mnemonic']=='SetPuppet':
                    ci=command['operands'].get('iValues',[])
                    if len(ci)>=3 and ci[1]==-1001: puppet_entries.append(ci[2])
            moves=[]
            for entry in puppet_entries:
                end=min([s['EntryPoint'] for s in cont.get('segments',[]) if s['EntryPoint']>entry] or [len(cont.get('mnemonics',[]))])
                for pos in cont.get('mnemonics',[])[entry:end]:
                    if pos['mnemonic'] in ('PuppetFin','PuppetEnd','Exit','Return'): break
                    if pos['mnemonic'] in ('SetPos','Lerp','MoveTo','Move','MoveBirdView'):
                        moves.append({'mnemonic':pos['mnemonic'],'operands':pos['operands']})
            if moves: position_changes+=1
            points=[p for p in objects if p['submap']==maps.get(iv[0]) and p['type_name']=='PointIn' and p['props'].get('object_id')==iv[1]]
            landing=None
            if len(points)==1:
                p=points[0]; landing=[b.to_int(p.get('tx')), b.tile_row(p), b.layer_index(p.get('layer'))]
                original=list(landing)
                for move in moves:
                    args=move['operands'].get('iValues',[])
                    if move['mnemonic'] in ('SetPos','Lerp') and len(args)>=4:
                        if args[3]==0:
                            if args[0]!=-1:landing[0]=args[0]
                            if args[1]!=-1:landing[1]=args[1]
                        elif args[3]==1 and len(args)>4 and args[4]==-1001:
                            landing[0]+=args[0];landing[1]+=args[1]
                        else:landing=None;break
                        if args[2]>0:landing[2]=args[2]-1
                    elif move['mnemonic']=='Move' and len(args)>=2 and 0<=args[0]<4:
                        dx,dy=[(0,-1),(1,0),(0,1),(-1,0)][args[0]]
                        landing[0]+=dx*args[1];landing[1]+=dy*args[1]
                    elif move['mnemonic']=='MoveTo' and len(args)>=4 and args[1]==0 and args[0] in (0,1):
                        landing[args[0]]=args[2]
                        if args[3]>0:landing[2]=args[3]-1
                    else:landing=None;break
            else:original=None
            changes.append({'map':iv[0],'point':iv[1],'continuation':cont_name,'player_position_commands':moves,'original_cell':original,'candidate_landing':landing})
        report.append({'game':game,'submap':o['submap'],'entity_id':o['props'].get('entity_id'),'name':name,
                       'script_id':o['props'].get('script_id'),'action_id':o['props'].get('action_id'),
                       'script_name':script.get('name'), 'command_kinds':sorted({m['mnemonic'] for m in script.get('mnemonics',[])}),'changes':changes})
    print(f'FF{n}: {candidates} named environmental traps; {position_changes} continuation branches with player position commands')
out=ROOT/'build_check/re/trap_arrival_audit.json'
out.parent.mkdir(parents=True,exist_ok=True)
out.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
for row in report:
    for change in row['changes']:
        if change['original_cell']!=change['candidate_landing']:
            print('LANDING DIFFERENCE',row['game'],row['submap'],row['entity_id'],row['name'],change['map'],change['point'],change['continuation'],change['original_cell'],'->',change['candidate_landing'])
