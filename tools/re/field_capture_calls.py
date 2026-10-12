"""Read-only PE evidence for field lifecycle and bounded transport capture."""
from pathlib import Path
from collections import defaultdict
import re
import hashlib
import capstone
import pefile

ROOT = Path(__file__).resolve().parents[2]
STEAM = Path('D:/Games/SteamLibrary/steamapps/common')
GAMES = ['FINAL FANTASY PR', 'FINAL FANTASY II PR', 'FINAL FANTASY III PR',
         'FINAL FANTASY IV PR', 'FINAL FANTASY V PR', 'FINAL FANTASY VI PR']
WANTED = {'FieldMap': {'StateInit'}, 'FieldPlayerKeyController': {'Init'},
          'FieldPlayerController': {'Init'},
          'TransportationController': {'NeedInteractiveList'},
          'Transportation': {'CheckNeedInteractive', 'GetInfoData'},
          'TransportationInfo': {'get_MapObject', 'get_Enable', 'get_Type', 'get_Id'},
          'FieldController': {'IsCanMoveToDestPosition', 'CreateFieldPlayer', 'SetControlPlayer'},
          'KeyHelpController': {'Initialize'}, 'BattlePlayerInfomationController': {'Initalize'},
          'ShopController': {'Show'}, 'ScreenTimerController': {'Initialize'},
          'FieldGrobalTimer': {'Init'}, 'ExtraSoundController': {'Initialize'},
          'LoadGameWindowController': {'SetActive'}}
for number, folder in enumerate(GAMES, 1):
    names, targets = defaultdict(list), {}
    dump = (ROOT.parent / ('ff%d' % number) / 'dump.cs').read_text(encoding='utf-8-sig')
    for block in dump.split('// Namespace: ')[1:]:
        cls = re.search(r'^(?:public|private|internal).*?\b(?:class|struct) ([\w.<>]+)', block, re.M)
        if not cls:
            continue
        for match in re.finditer(r'// RVA: 0x([0-9A-F]+)[^\n]*\n\s*(?:public|private|protected|internal) [^\n]*? ([\w.<>]+)\(', block):
            rva, method = int(match[1], 16), match[2]
            label = cls[1] + '.' + method
            names[rva].append(label)
            namespace = block.splitlines()[0]
            expected = {'KeyHelpController':'Last.UI.KeyInput', 'BattlePlayerInfomationController':'Last.UI.KeyInput',
                        'ShopController':'Last.UI.KeyInput', 'ExtraSoundController':'Last.UI.KeyInput',
                        'LoadGameWindowController':'Last.UI.KeyInput', 'ScreenTimerController':'Last.UI',
                        'FieldGrobalTimer':'Last.UI.Common.Map'}
            if method in WANTED.get(cls[1], set()) and (cls[1] not in expected or namespace == expected[cls[1]]):
                targets[label] = rva
    binary = STEAM / folder / 'GameAssembly.dll'
    pe = pefile.PE(str(binary), fast_load=True)
    ordered = sorted(names)
    dis = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    output = ['GameAssembly SHA256: ' + hashlib.sha256(binary.read_bytes()).hexdigest()]
    for label, rva in targets.items():
        end = next((address for address in ordered if address > rva), rva + 4096)
        output.append('\n%s RVA %X' % (label, rva))
        for ins in dis.disasm(pe.get_data(rva, min(end - rva, 32768)), rva):
            note = ''
            if ins.mnemonic in ('call', 'jmp') and ins.op_str.startswith('0x'):
                labels = names.get(int(ins.op_str, 16), [])
                note = ' ; ' + (' / '.join(labels) if len(labels) < 7 else 'folded %d aliases' % len(labels))
            output.append('%X %s %s%s' % (ins.address, ins.mnemonic, ins.op_str, note))
    required = ('FieldMap.StateInit', 'FieldPlayerKeyController.Init',
                'TransportationController.NeedInteractiveList', 'Transportation.CheckNeedInteractive',
                'FieldController.IsCanMoveToDestPosition', 'FieldController.CreateFieldPlayer',
                'FieldController.SetControlPlayer', 'KeyHelpController.Initialize',
                'BattlePlayerInfomationController.Initalize', 'ShopController.Show',
                'ExtraSoundController.Initialize', 'LoadGameWindowController.SetActive')
    if number >= 4:
        required += ('ScreenTimerController.Initialize', 'FieldGrobalTimer.Init')
    for label in required:
        assert label in targets, 'FF%d missing %s' % (number, label)
    (ROOT / 'docs/re' / ('ff%d_field_capture.txt' % number)).write_text('\n'.join(output) + '\n', encoding='utf-8')
    print('FF%d: lifecycle/transport/collision bodies recorded (%d functions)' % (number, len(targets)))
