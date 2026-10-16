"""Offline opening-enemy producer and rare encounter timing-boundary audit, all six games.

Reads matching local dumps/GameAssembly only. No launch, attach or game calls.
Stores reproducible RVA/body/hash evidence and bounded disassembly for inspection.
"""
import hashlib
import json
import os
from pathlib import Path
import re
from collections import Counter
import capstone
import pefile
import UnityPy

ROOT = Path(os.path.abspath(__file__)).parents[2]
STEAM = Path('D:/Games/SteamLibrary/steamapps/common')
GAMES = ['FINAL FANTASY PR', 'FINAL FANTASY II PR', 'FINAL FANTASY III PR',
         'FINAL FANTASY IV PR', 'FINAL FANTASY V PR', 'FINAL FANTASY VI PR']
TARGETS = [
    ('Last.Battle', 'BattleController', 'StartPreeMptiveMes', 0),
    ('Last.Battle', 'BattleController', 'StartInit', 0),
    ('Last.Battle', 'BattleController', 'StartCreateObj', 0),
    ('Last.Battle', 'BattleController', 'StartInBattle', 0),
    ('Last.Battle', 'BattleController', 'InitBattleData', 0),
    ('Last.Battle', 'BattleController', 'InitEnemyStatus', 0),
    ('Last.Battle', 'BattleController', 'UpdateUI', 0),
    ('Last.UI', 'BattleUIManager', 'SetData', 3),
    ('Last.UI.KeyInput', 'BattleEnemyInfomationController', 'Initalize', 0),
    ('Last.UI.KeyInput', 'BattleEnemyInfomationController', 'UpdateView', 2),
    ('Last.UI.KeyInput', 'BattleEnemyInfomationController', 'GroupingById', 2),
    ('Last.UI.KeyInput', 'BattleEnemyInfomationContentController', 'UpdateView', 1),
    ('Last.InGame.Effects', 'EncountEffectManager', 'SetAnimation', 6),
]


def main():
    output = ROOT / 'build_check/battle_entry_2026_10_16'
    output.mkdir(parents=True, exist_ok=True)
    manifest = []
    for n, game in enumerate(GAMES, 1):
        dump_path = ROOT.parent / ('ff%d/dump.cs' % n)
        binary_path = STEAM / game / 'GameAssembly.dll'
        dump = dump_path.read_text(encoding='utf-8-sig')
        functions, classes, signatures = [], {}, {}
        for block in dump.split('// Namespace: ')[1:]:
            namespace = block.splitlines()[0].strip()
            cls = re.search(r'^.*?\b(?:class|struct) ([\w.<>]+)', block, re.M)
            if not cls:
                continue
            classes[namespace, cls[1]] = block
            for method in re.finditer(r'^\s*// RVA: 0x([0-9A-F]+)[^\n]*\n\s*(?:public|private|protected|internal) [^\n]*? ([\w.<>]+)\(([^\n]*)\)', block, re.M):
                args = method[3]
                # These audited signatures contain no nested generic commas.
                function = (int(method[1], 16), namespace, cls[1], method[2], 0 if not args else args.count(',')+1)
                functions.append(function)
                signatures[function] = method[0].splitlines()[-1].strip()
        counts = Counter(f[0] for f in functions)
        names = {}
        for f in functions:
            names.setdefault(f[0], []).append('.'.join(f[1:4]))
        pe = pefile.PE(str(binary_path), fast_load=True)
        image = pe.get_memory_mapped_image()
        boundaries = sorted({f[0] for f in functions})
        decoder = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
        record = {'game': n, 'dump_sha256': hashlib.sha256(dump_path.read_bytes()).hexdigest(),
                  'game_sha256': hashlib.sha256(binary_path.read_bytes()).hexdigest(), 'methods': [], 'fields': {}}
        phases = dump.split('enum BattleController.BattleState ', 1)[1].split('\n}', 1)[0]
        record['opening_phase_enum'] = dict(re.findall(r'public const BattleController.BattleState (\w+) = (\d+)', phases)[:6])
        assert record['opening_phase_enum'] == {'None':'0','Init':'1','CreateObj':'2','InBattle':'3','PreeMptiveMes':'4','Play':'5'}
        lines = []
        for target in TARGETS:
            matches = [f for f in functions if f[1:] == target]
            assert len(matches) == 1, (n, target, matches)
            rva = matches[0][0]
            unique = counts[rva] == 1
            assert unique, ('folded target', n, target, counts[rva])
            end = next((v for v in boundaries if v > rva), rva+32768)
            assert 0 < end-rva <= 32768, ('unexpected body bound', n, target, end-rva)
            body = image[rva:end]
            label = '.'.join(target[:3])
            calls = []
            lines.append('\n%s 0x%X\n' % (label, rva))
            for instruction in decoder.disasm(body, rva):
                note = ''
                if instruction.mnemonic in ('call', 'jmp') and instruction.op_str.startswith('0x'):
                    address = int(instruction.op_str, 16)
                    callee = names.get(address, [])
                    note = ' ; '+(' | '.join(callee) if len(callee) <= 3 else '%d folded methods' % len(callee))
                    if callee:
                        calls.append({'rva': hex(address), 'methods': callee[:3], 'folded_count': len(callee)})
                lines.append('%X %s %s%s\n' % (instruction.address, instruction.mnemonic, instruction.op_str, note))
            record['methods'].append({'name': label, 'argc': target[3], 'rva': hex(rva), 'unique': unique,
                                      'signature': signatures[matches[0]],
                                      'bytes': len(body), 'body_sha256': hashlib.sha256(body).hexdigest(), 'calls': calls})
        for cls, fields in [('BattleEnemyInfomationController', ['contentList']),
                            ('BattleEnemyInfomationContentController', ['view']),
                            ('BattleEnemyInfomationContentView', ['nameText', 'countText'])]:
            block = classes['Last.UI.KeyInput', cls]
            record['fields'][cls] = {}
            for field in fields:
                found = re.search(r'^\s*private ([^\n]+?) '+field+r'; // (0x[0-9A-F]+)', block, re.M)
                assert found, (n, cls, field)
                record['fields'][cls][field] = {'type': found[1], 'offset': found[2]}
        data = next(p for p in (STEAM/game).iterdir() if p.is_dir() and p.name.endswith('_Data'))
        bundles = list((data/'StreamingAssets/aa/StandaloneWindows64').glob('key_battle_assets_all_*.bundle'))
        assert len(bundles) == 1
        env = UnityPy.load(str(bundles[0]))
        scripts, trees = {}, {}
        for obj in env.objects:
            if obj.type.name == 'MonoScript':
                script = obj.read()
                scripts[obj.path_id] = (script.m_Namespace, script.m_ClassName)
            elif obj.type.name == 'MonoBehaviour':
                trees[obj.path_id] = obj.read_typetree()
        views = {pid: tree for pid, tree in trees.items()
                 if scripts.get(tree.get('m_Script', {}).get('m_PathID')) ==
                    ('Last.UI.KeyInput', 'BattleEnemyInfomationContentView')}
        assert views, (n, 'No serialized enemy name/count view')
        templates = []
        for pid, view in views.items():
            for field in ['nameText', 'countText']:
                assert view[field]['m_PathID'] in trees, (n, 'Missing text field', field)
                text_view = trees[view[field]['m_PathID']]
                assert scripts[text_view['m_Script']['m_PathID']] == ('UnityEngine.UI', 'Text')
            controllers = [tree for tree in trees.values()
                           if scripts.get(tree.get('m_Script', {}).get('m_PathID')) ==
                              ('Last.UI.KeyInput', 'BattleEnemyInfomationContentController')
                           and tree.get('view', {}).get('m_PathID') == pid]
            assert len(controllers) == 1
            assert controllers[0]['m_GameObject'] == view['m_GameObject']
            templates.append({'view_path_id': pid, 'controller_and_view_share_game_object': True,
                              'nameText': 'UnityEngine.UI.Text', 'countText': 'UnityEngine.UI.Text'})
        record['assets'] = {'bundle': bundles[0].name,
                            'sha256': hashlib.sha256(bundles[0].read_bytes()).hexdigest(), 'enemy_templates': templates}
        (output / ('ff%d_native.txt' % n)).write_text(''.join(lines), encoding='utf-8')
        manifest.append(record)
        print('FF%d: %d unique native opening/encounter producers; assigned name/count fields verified' % (n, len(TARGETS)))
    (output / 'native_manifest.json').write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf-8')
    durable = json.loads(json.dumps(manifest))
    for game in durable:
        for method in game['methods']:
            method.pop('calls')
    (ROOT / 'docs/re/battle_entry_native.json').write_text(json.dumps(durable, indent=2)+'\n', encoding='utf-8')


if __name__ == '__main__':
    main()
