"""Bounded all-game party/formation/battle display evidence from the matching PE; never executes it."""
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
 ('Last.Map','FieldController'):{'GetCorpsListCloneWithApparentOrder'},
 ('Last.UI.KeyInput','StatusWindowController'):{'SwitchCorps','SelectContent','ContentsUpdate','SortSourceSelectInit','SortTargetSelectInit','SetKeyHelp'},
 ('Serial.Template.UI.KeyInput','StatusWindowControllerBase'):{'SetMenuCommandId','SelectFunc','ShiftDownCharacterCorpId','ShiftUpCharacterCorpsId'},
 ('Serial.FF0.UI.KeyInput' if NUMBER==4 else 'Serial.FF%d.UI.KeyInput'%NUMBER,'StatusWindowContentController'):{'SetCorps','SetImageFront','SetImageBack','SetSortView','SetDefaultView'},
 ('Last.UI.KeyInput','BattlePlayerInfomationController'):{'Initalize','UpdateView','SetFocus','SetTargetFocus','UsePlayerInfoContent','UseTargetSelectContent'},
 ('Last.UI.KeyInput','BattlePlayerInfomationContentController'):{'UpdateView'},
 ('Last.UI.KeyInput','BattlePlayerInfomationTargetSelectContentController'):{'UpdateView'},
 ('Last.UI.KeyInput','BattleTargetSelectController'):{'PlayerInit','SelectContent'},
 ('Last.UI.KeyInput','BattleItemInfomationController'):{'SelectContent','UseSelectInit','UseSelectExit','Close'},
 ('Last.UI.KeyInput','BattleCommandSelectController'):{'SetCursor'},
}
source = (ROOT.parent / ('ff%d/dump.cs' % NUMBER)).read_text(encoding='utf-8-sig')
names, targets, schemas = {}, [], []
for block in source.split('// Namespace: ')[1:]:
    ns = block.splitlines()[0]
    cls = re.search(r'^(?:public|private|internal).*?\b(?:class|struct) ([\w.<>]+)', block, re.M)
    if not cls:
        continue
    key = (ns, cls[1])
    if cls[1] in {c for _, c in TARGETS} or cls[1] in {'MagicStoneContentView', 'MagicStoneListView', 'AbilityUseContentListView', 'MessageSelectView', 'MessageSelectContentView'}:
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
(dest / ('ff%d_party_display_calls.txt' % NUMBER)).write_text('\n'.join(out) + '\n', encoding='utf-8')
(dest / ('ff%d_party_display_schema.txt' % NUMBER)).write_text('\n'.join(schemas), encoding='utf-8')
print('ff%d party/display: %d matching functions; SHA256 %s'%(NUMBER,len(targets),hashlib.sha256(binary.read_bytes()).hexdigest()))
