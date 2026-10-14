"""Bounded FF6 producer/control evidence from the matching PE; never executes it."""
import hashlib
import pathlib
import re
import sys
import capstone
import pefile

ROOT = pathlib.Path(__file__).resolve().parents[2]
NUMBER = int(sys.argv[1]) if len(sys.argv) > 1 else 6
assert 1 <= NUMBER <= 6
TARGETS = {
    ('Last.Map', 'FieldPlayerKeyAirshipController'): {'Init', 'LateUpdateObserveInput', 'MoveToAxis', 'OnTouchPadCallback', 'OnKey', 'OnKeyUp', 'SwitchInputEnable', '.ctor'},
    ('Last.Map', 'FieldPlayerController'): {'Init', '.ctor'},
    ('Last.Entity.Field', 'FieldEntity'): {'get_IsMoving', 'get_IsMovingDestPosition'},

    ('Last.UI.KeyInput', 'BattleTargetSelectController'): {
        'EnemysInit', 'EnemysUpdate', 'PlayerInit', 'PlayerUpdate', 'SelectContent',
        'SetActiveCursor', 'PlayerAllInit', 'EnemyAllInit', 'AllInit', 'EnemyAllUpdate', 'ShowWindow',
        'SideAttackPlayerAllInit', 'SideAttackEnemyAllInit', 'EnemySideAttackPlayerAllInit', 'EnemySideAttackEnemyAllInit'},
    ('Last.UI.KeyInput', 'MessageSelectController'): {'Show', 'Close', 'SetForcusIndex'},
    ('Last.UI.Message', 'MessageSelectController'): {'Show', 'Hide', 'SelectInit', 'SetCursor', 'SelectContent', 'SetData'},
    ('Serial.FF6.UI.KeyInput', 'AbilityUseContentListController'): {'Show', 'SelectContent', 'SetCursor', 'ResetCursor', 'WaitInputInit'},
    ('Serial.FF6.UI.KeyInput', 'AbilityWindowController'): {'UseTargetInit', 'UseListInit', 'UpdateController', 'SelectMagicStoneInit', 'MagicStoneDetailsInit'},
    ('Serial.FF6.UI.KeyInput', 'AbilityContentListController'): {'ResetCursor', 'SetCursor', 'SelectContent'},
    ('Serial.FF6.UI.KeyInput', 'JobChangeWindowController'): {'UpdateJobInfo'},
    ('Serial.Template.UI', 'JobChangeWindowBaseController'): {'UpdateJobEquipView', 'UpdateJobEquipIconView'},
    ('Last.UI.KeyInput', 'ItemEquipmentController'): {'SetPage', 'UpdateView', 'SetNextPage', 'SetPrevPage', 'UpdateController', 'SetParameterFlag', 'SetDescriptionText', 'SwitchParameter', 'Show', 'WaitInputInit', 'WaitInputUpdate'},
    ('', 'ItemEquipmentController.<StartPrevPage>d__22'): {'MoveNext'},
    ('', 'ItemEquipmentController.<StarNextPage>d__23'): {'MoveNext'},
    ('Last.UI.KeyInput', 'PagerController'): {'SetPage', 'SetCurrentPage'},
    ('Last.UI.KeyInput', 'ItemWindowController'): {'EquipmentInit', 'UpdateController'},
    ('Last.UI.KeyInput', 'ItemEquipableController'): {'SetData'},
    ('Last.UI.KeyInput', 'ItemEquipableContentController'): {'UpdateView'},
    ('Serial.FF6.UI.KeyInput', 'MagicStoneListController'): {'SelectContent', 'ContentUpdate', 'ConfimePopupInit', 'Show', 'WaitInputInit', 'WaitInputUpdate', 'Hide'},
    ('Serial.FF6.UI.KeyInput', 'MagicStoneDetailsController'): {'Show', 'Hide', 'WaitInputInit', 'WaitInputUpdate', 'UpdateView'},
    ('Serial.FF6.UI', 'MagicStoneDetailsControllerBase'): {'SetState'},
    ('Serial.FF6.UI', 'MagicStoneListControllerBase'): {'ContentUpdate', 'UpdateFocus'},
    ('Serial.FF6.UI.KeyInput', 'MagicStoneListContentController'): {'EnableEquipIcon', 'EnableTargetEquipIcon'},
    ('Last.UI.KeyInput', 'EquipmentSelectWindowController'): {'<UpdateItemList>b__36_0', 'SelectContent'},
    ('Last.UI.KeyInput', 'ItemListContentController'): {'SetData'},
    ('Serial.Template.UI.KeyInput', 'MagicStoneListControllerBase'): {'SelectContent', 'ConfirmInit', 'SetData'},
    ('Last.UI.KeyInput', 'MagicStoneListController'): {'SelectContent', 'ConfirmInit', 'SetData'},
    ('Serial.FF6.UI', 'StatusDetailsView'): {'SetBattleCommand', 'SetData'},
    ('Serial.Template.Management', 'MagicStoneProvider'): {'IsMasterLearnableAbility', 'GetLearnableAbilityList'},
    ('Serial.FF6.Management', 'MagicStoneData'): {'IsMasterLearnableAbility'},
}
TARGETS = {(ns.replace('FF6', 'FF%d' % NUMBER), cls): methods for (ns, cls), methods in TARGETS.items()}
TARGETS[('Serial.FF0.UI.KeyInput' if NUMBER == 4 else 'Serial.FF%d.UI.KeyInput' % NUMBER,
         'BattleQuantityAbilityInfomationController')] = {'ShowUseSelect', 'UseSelectInit', 'SelectContent', 'UpdateView'}
source = (ROOT.parent / ('ff%d/dump.cs' % NUMBER)).read_text(encoding='utf-8-sig')
names, targets, schemas = {}, [], []
for block in source.split('// Namespace: ')[1:]:
    ns = block.splitlines()[0]
    cls = re.search(r'^(?:public|private|internal).*?\b(?:class|struct) ([\w.<>]+)', block, re.M)
    if not cls:
        continue
    key = (ns, cls[1])
    if cls[1] in {c for _, c in TARGETS} or cls[1] in {'MagicStoneContentView', 'MagicStoneListView', 'MagicStoneListContentView', 'MagicStoneDetailsView', 'MagicStoneListData', 'ItemEquipmentView', 'PagerView', 'AbilityUseContentListView', 'MessageSelectView', 'MessageSelectContentView'}:
        schemas.append('// Namespace: ' + block)
    for entry in re.finditer(r'^\s*// RVA: 0x([0-9A-F]+)[^\n]*\n\s*(?:public|private|protected|internal) [^\n]*? ([\w.<>]+)\(([^\n]*)\)', block, re.M):
        rva, method, args = int(entry[1], 16), entry[2], entry[3]
        name = '.'.join((*key, method))
        names.setdefault(rva, []).append(name)
        if method in TARGETS.get(key, set()):
            targets.append((name, args, rva))
folders = ['FINAL FANTASY PR', 'FINAL FANTASY II PR', 'FINAL FANTASY III PR',
           'FINAL FANTASY IV PR', 'FINAL FANTASY V PR', 'FINAL FANTASY VI PR']
binary = pathlib.Path('D:/Games/SteamLibrary/steamapps/common') / folders[NUMBER-1] / 'GameAssembly.dll'
pe = pefile.PE(str(binary), fast_load=True)
ordered = sorted(names)
dis = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
out = ['GameAssembly SHA256: ' + hashlib.sha256(binary.read_bytes()).hexdigest()]
for name, args, rva in targets:
    end = next((x for x in ordered if x > rva), rva + 4096)
    out.append(f'\n{name}({args}) RVA {rva:X}; aliases {len(names[rva])}')
    for ins in dis.disasm(pe.get_data(rva, min(end-rva, 16384)), rva):
        annotation = ''
        if ins.mnemonic in ('call', 'jmp') and ins.op_str.startswith('0x'):
            labels = names.get(int(ins.op_str, 16), [])
            annotation = ' ; ' + (' / '.join(labels) if len(labels) <= 5 else f'folded ({len(labels)} aliases)')
        out.append(f'{ins.address:X} {ins.mnemonic} {ins.op_str}{annotation}')
dest = ROOT / 'build_check/re'
dest.mkdir(parents=True, exist_ok=True)
(dest / ('ff%d_followup_calls.txt' % NUMBER)).write_text('\n'.join(out) + '\n', encoding='utf-8')
(dest / ('ff%d_followup_schema.txt' % NUMBER)).write_text('\n'.join(schemas), encoding='utf-8')
print(out[0])
for name, args, rva in targets:
    print(f'{rva:X} {name}({args}); aliases {len(names[rva])}')
