"""Build conservative mod descriptions for every extracted Events-category candidate.

Uses offline map objects/script command kinds, never spoken dialogue or future outcomes.
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
}
SAFE = set(NOUNS) | {'Party chat', 'Conversation', 'Scene trigger',
                    'Event trigger', 'Interactive object', 'Layer transition',
                    'Warp tile', 'Vehicle interaction', 'Switch trigger'}


def load(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def infer(o, commands):
    t, raw = o['type'], o.get('name', '')
    if t == 4: return 'Layer transition', 'native layer-transition type'
    if t == 20: return 'Warp tile', 'native teleport type'
    if t == 21: return 'Vehicle interaction', 'native vehicle action type'
    if t == 30: return 'Switch trigger', 'native switch-event type'
    name = unicodedata.normalize('NFKC', raw).lower()
    if any(word in name for word in ('party chat', 'パーティ会話', 'パーティー会話')):
        return 'Party chat', 'conversation identifier; actors/outcome withheld'
    if any(word in name for word in ('conversation', ' talks ', ' speaks ', '会話', '話しかけ')):
        return 'Conversation', 'conversation identifier; actors/outcome withheld'
    # A bare scenery noun can describe an inspection point. Narrative identifiers
    # containing the noun do not qualify: e.g. a crystal being stolen stays a scene.
    if t in {5, 10}:
        bare = re.sub(r'^(?:sc)?\d+[:.]', '', name)
        bare = re.sub(r'[\d_\s]+$', '', bare).strip()
        for label, words in NOUNS.items():
            if bare in words: return label, 'bare interactible-object noun'
    if any(c == 'Msg' or 'Message' in c or 'Dialog' in c for c in commands):
        return 'Conversation', 'script presents dialogue; text/participants withheld'
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
    audit = []
    for o in objects:
        if o['type'] not in TYPES: continue
        p = o.get('props') or {}
        script = scripts.get((o['submap'], masters.get(p.get('script_id'), '')), {})
        commands = {m['mnemonic'] for m in (script.get('mnemonics') or [])}
        label, reason = infer(o, commands)
        if o['name'] in overrides:
            label, reason = overrides[o['name']], 'reviewed exact-name override'
            assert label in SAFE, f'Unknown safe label: {label}'
        choices[o['name']].add(label)
        audit.append(dict(submap=o['submap'], group=o['group'], id=o['id'], raw=o['name'],
                          type=o['type'], label=label, reason=reason))
    # Shared developer names can stand for different contexts; avoid choosing one story.
    labels = {name: next(iter(values)) if len(values) == 1 else 'Event trigger'
              for name, values in sorted(choices.items()) if name}
    (ROOT / 'data' / game / 'eventlabels.json').write_text(json.dumps(labels, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    (ROOT / 'docs' / 're' / f'{game}_event_label_audit.json').write_text(json.dumps(audit, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    counts = collections.Counter(labels.values())
    print(f'FF{n}: {len(audit)} candidates, {len(labels)} labels; ' + ', '.join(f'{k} {v}' for k, v in sorted(counts.items())))


if __name__ == '__main__':
    for n in range(1, 7): build(n)
