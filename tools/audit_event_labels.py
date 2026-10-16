"""Build conservative mod descriptions for every extracted Events-category candidate.

Uses offline developer identifiers; a Msg opcode alone never implies conversation.
Reviewed text may establish an object's identity, never a reward or future outcome.
Input: ../tools/mapdump. Output: per-game eventlabels.json and an inference audit.
Exact per-name overrides can be added to event_label_overrides.json after review.
"""
import collections
import csv
import json
import os
from pathlib import Path
import re
import unicodedata

ROOT = Path(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DUMP = ROOT.parent / 'tools' / 'mapdump'
TYPES = {2, 4, 5, 10, 20, 21, 28, 30}
NOUNS = {
    'Door': ('door', '扉', 'ドア'), 'Switch': ('switch', 'スイッチ'),
    'Lever': ('lever', 'レバー'), 'Button': ('button', 'ボタン'),
    'Clock': ('clock', '時計'), 'Bookshelf': ('bookshelf', '本棚'),
    'Book': ('book', '本'), 'Sign': ('sign', '看板', '立て札'),
    'Bed': ('bed', 'ベッド'), 'Piano': ('piano', 'ピアノ'),
    'Painting': ('painting', '絵画'), 'Mirror': ('mirror', '鏡'),
    'Well': ('well', '井戸'), 'Fountain': ('fountain', '噴水'),
    'Statue': ('statue', '像'), 'Altar': ('altar', '祭壇'),
    'Ladder': ('ladder', '梯子', 'はしご'), 'Stairs': ('stairs', '階段'),
    'Barrel': ('barrel', '樽'), 'Pot': ('pot', '壺'),
    'Crystal': ('crystal', 'クリスタル'), 'Gravestone': ('gravestone', '墓石'),
}
SAFE = set(NOUNS) | {'Party chat', 'Conversation', 'Scene trigger', 'Trap',
                    'Event trigger', 'Interactive object', 'Layer transition',
                    'Warp Tile', 'Vehicle interaction', 'Switch trigger', 'Magic Circle', 'Dragoon'}


def trap_identifier(raw):
    name = raw.lower()
    if any(word in name for word in ('落とし穴','落し穴','穴に落ちる','地割れに落ちる',
            'pitfall','trapdoor','floor_break','floorbreak','bridge_break','bridgebreak',
            'floor_crack','crack_floor','cracked_floor','position_reset','reset_position','room_reset')):
        return True
    body = re.sub(r'^(?:sc_|ev_)[^:：]*[:：]', '', name)
    if re.fullmatch(r'落ちる[0-9０-９①-⑳㉑-㉟㊱-㊿]*', body): return True
    if body.startswith(('底なし沼に落ち', '滝に押し戻される')): return True
    if (body.startswith('map_') or re.match(r'^[0-9]',body)) and 'に落下' in body: return True
    return body in ('屋外に落っこちる','部屋に落っこちる','crack','fall','collapsing_floor','collapsing_bridge') or body.startswith('fall_')


def load(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def infer(o, commands, script_name=''):
    t, raw = o['type'], o.get('name', '')
    if t == 4: return 'Layer transition', 'native layer-transition type'
    if t == 20: return 'Event trigger', 'Teleport return-cache metadata, excluded from objectives'
    if t == 21: return 'Vehicle interaction', 'native vehicle action type'
    if t == 30: return 'Switch trigger', 'native switch-event type'
    name = unicodedata.normalize('NFKC', raw).lower()
    if t in {2, 28} and trap_identifier(raw):
        return 'Trap', 'environmental pit/drop identifier; story and reward withheld'
    if re.search(r'(?:ワープ(?:地点[A-Z]|装置|する.*)?$|(?:へ|に)ワープ$)',raw) or name.strip() in {'warp','teleport','warp pad'}:
        return 'Warp Tile', 'explicit travel identifier; no trap/failure evidence'
    if 'ChangeMap' in commands and re.search(r'(?:^sc_war_|(?:^|_)warp(?:_|$))',script_name,re.I):
        return 'Warp Tile', 'explicit script travel identifier; no trap/failure evidence'
    if any(word in name for word in ('party chat', 'パーティ会話', 'パーティー会話')):
        return 'Party chat', 'conversation identifier; actors/outcome withheld'
    if any(word in name for word in ('conversation', ' talks ', ' speaks ', '会話', '話しかけ')):
        return 'Conversation', 'conversation identifier; actors/outcome withheld'
    # A bare scenery noun can describe an inspection point. Narrative identifiers
    # containing the noun do not qualify: e.g. a crystal being stolen stays a scene.
    if t in {5, 10}:
        bare = re.sub(r'^(?:sc_|ev_)[^:：]*[:：]', '', name)
        bare = re.sub(r'^(?:sc)?\d+[:.]', '', bare)
        bare = re.sub(r'[\d_\s]+$', '', bare).strip()
        for label, words in NOUNS.items():
            if bare in words: return label, 'bare interactible-object noun'
    # Descriptions, corpses, signs, doors, rewards and cutscenes also use Msg.
    # Unknown identities stay neutral instead of inventing a conversation.
    if o.get('props', {}).get('script_id'):
        return 'Scene trigger' if t in {2, 28} else 'Interactive object', 'script action; outcome withheld'
    return ('Event trigger' if t in {2, 28} else 'Interactive object'), 'neutral fallback; no supported inference'


def build(n):
    game = f'ff{n}'
    objects = [json.loads(s) for s in (DUMP / f'{game}_objects.jsonl').read_text(encoding='utf-8').splitlines()]
    scripts = {(s['submap'], s['name']): s for s in
               (json.loads(l) for l in (DUMP / f'{game}_scripts.jsonl').read_text(encoding='utf-8').splitlines())}
    with (DUMP / f'{game}_master' / 'script.txt').open(encoding='utf-8-sig', newline='') as f:
        masters = {int(r['id']): r['script_name'] for r in csv.DictReader(l for l in f if l.strip()) if r['id']}
    override_path = ROOT / 'data' / game / 'event_label_overrides.json'
    overrides = load(override_path) if override_path.exists() else {}
    choices = collections.defaultdict(set)
    descriptions = {}
    message_file = DUMP / f'{game}_master' / 'story_mes_en.txt'
    if message_file.exists():
        descriptions = dict(line.split('\t',1) for line in message_file.read_text(encoding='utf-8-sig').splitlines() if '\t' in line)
    audit = []
    for o in objects:
        if o['type'] not in TYPES: continue
        p = o.get('props') or {}
        script_name = masters.get(p.get('script_id'), '')
        script = scripts.get((o['submap'], script_name), {})
        commands = {m['mnemonic'] for m in (script.get('mnemonics') or [])}
        label, reason = infer(o, commands, script_name)
        if o['name'] in overrides:
            label, reason = overrides[o['name']], 'reviewed exact-name override'
            assert label in SAFE, f'Unknown safe label: {label}'
            if label == 'Dragoon':
                assert game=='ff2' and o['submap']=='Map_30071_2' and o['group']=='ev_e_0049' and p['entity_id']==32 and script_name=='sc_e_0049'
                message = next(m['operands']['sValues'][0] for m in script['mnemonics'] if m['mnemonic']=='Msg')
                assert descriptions[message].startswith('You come across the remains of a dragoon.')
        if o['type'] != 20: choices[o['name']].add(label)
        audit.append(dict(submap=o['submap'], group=o['group'], id=o['id'], raw=o['name'],
                          type=o['type'], label=label, reason=reason,
                          trap_hint=trap_identifier(o['name'])))
    # Shared developer names can stand for different contexts; avoid choosing one story.
    labels = {name: next(iter(values)) if len(values) == 1 else 'Event trigger'
              for name, values in sorted(choices.items()) if name}
    (ROOT / 'data' / game / 'eventlabels.json').write_text(json.dumps(labels, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    (ROOT / 'docs' / 're' / f'{game}_event_label_audit.json').write_text(json.dumps(audit, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    counts = collections.Counter(labels.values())
    print(f'FF{n}: {len(audit)} candidates, {len(labels)} labels; ' + ', '.join(f'{k} {v}' for k, v in sorted(counts.items())))


if __name__ == '__main__':
    for n in range(1, 7): build(n)
