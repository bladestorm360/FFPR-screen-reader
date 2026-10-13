"""Bounded read-only FF2 weapon-icon producer evidence; raw code stays local."""
from pathlib import Path
from collections import defaultdict
import hashlib
import re
import capstone
import pefile

ROOT = Path(__file__).resolve().parents[2]
BINARY = Path('D:/Games/SteamLibrary/steamapps/common/FINAL FANTASY II PR/GameAssembly.dll')
OUTPUT = ROOT / 'build_check/re/ff2_weapon_icon.txt'
names, targets = defaultdict(list), {}
for block in (ROOT.parent / 'ff2/dump.cs').read_text(encoding='utf-8-sig').split('// Namespace: ')[1:]:
    namespace = block.splitlines()[0]
    cls = re.search(r'^(?:public|private|internal).*?\b(?:class|struct) ([\w.<>]+)', block, re.M)
    if not cls:
        continue
    for match in re.finditer(r'// RVA: 0x([0-9A-F]+)[^\n]*\n\s*(?:public|private|protected|internal) ([^\n]*? ([\w.<>]+)\([^\n]*)', block):
        rva, method = int(match[1], 16), match[3]
        label = namespace + '.' + cls[1] + '.' + method
        names[rva].append(label)
        if (namespace == 'Serial.FF2.UI' and cls[1] == 'SkillLevelContentController' and
                method in ('Initialize', 'SetWeaponIcon', 'SetAbilityIcon', 'SetBothHandsData', 'SetSkillLevelTargetType', 'UpdateView')):
            targets[label + '.' + format(rva, 'X')] = rva
        if namespace == 'Last.UI' and cls[1] == 'IconTextView' and method == 'SetIconImage':
            targets[label] = rva
assert len(targets) == 8, len(targets)
pe = pefile.PE(str(BINARY), fast_load=True)
ordered = sorted(names)
dis = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
output = ['GameAssembly SHA256: ' + hashlib.sha256(BINARY.read_bytes()).hexdigest()]
edges = {}
for label, rva in targets.items():
    end = next(address for address in ordered if address > rva)
    assert 0 < end - rva <= 8192
    output.append('\n' + label)
    calls = []
    called_addresses = []
    for ins in dis.disasm(pe.get_data(rva, end-rva), rva):
        labels = []
        if ins.mnemonic in ('call', 'jmp') and ins.op_str.startswith('0x'):
            called_addresses.append(int(ins.op_str, 16))
            labels = names.get(called_addresses[-1], [])
            if labels:
                calls.extend(labels)
        output.append(f'{ins.address:X} {ins.mnemonic} {ins.op_str}' + (' ; ' + ' / '.join(labels[:6]) if labels else ''))
    edges[rva] = called_addresses
    print(label + ': ' + ', '.join(dict.fromkeys(call for call in calls if '.IconText' in call or '.SkillLevel' in call)))
assert 0x4EC680 in edges[0x3DBF80] and 0x3DC460 not in edges[0x3DBF80], 'results icon producer changed'
assert 0x4EC680 in edges[0x3DC460], 'status icon producer changed'
OUTPUT.parent.mkdir(parents=True, exist_ok=True)
OUTPUT.write_text('\n'.join(output) + '\n', encoding='utf-8')
