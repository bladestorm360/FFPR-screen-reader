"""Read-only matching PE call evidence for native naming entry/focus ordering."""
from pathlib import Path
from collections import defaultdict
import hashlib
import re
import capstone
import pefile

ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / 'build_check/re'
OUTPUT.mkdir(parents=True, exist_ok=True) # local commercial-code evidence, never public source
STEAM = Path('D:/Games/SteamLibrary/steamapps/common')
GAMES = ['FINAL FANTASY PR', 'FINAL FANTASY II PR', 'FINAL FANTASY III PR']
for number, folder in enumerate(GAMES, 1):
    names, targets = defaultdict(list), {}
    dump = (ROOT.parent / f'ff{number}' / 'dump.cs').read_text(encoding='utf-8-sig')
    for block in dump.split('// Namespace: ')[1:]:
        cls = re.search(r'^(?:public|private|internal).*?\b(?:class|struct) ([\w.<>]+)', block, re.M)
        if not cls:
            continue
        for match in re.finditer(r'// RVA: 0x([0-9A-F]+)[^\n]*\n\s*(?:public|private|protected|internal) [^\n]*? ([\w.<>]+)\(', block):
            rva, method = int(match[1], 16), match[2]
            label = cls[1] + '.' + method
            names[rva].append(label)
            if (cls[1], method) in [('NewGameWindowController', 'InitNameSelect'), ('NameContentListController', 'SetFocus'), ('NameContentListController', 'UpdateView'), ('NameContentListController', 'SetEnbaleCursor')]:
                targets[label] = rva
    binary = STEAM / folder / 'GameAssembly.dll'
    pe = pefile.PE(str(binary), fast_load=True)
    ordered = sorted(names)
    dis = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    output = ['GameAssembly SHA256: ' + hashlib.sha256(binary.read_bytes()).hexdigest()]
    for label, rva in targets.items():
        end = next((address for address in ordered if address > rva), rva + 4096)
        output.append(f'\n{label} RVA {rva:X}')
        for ins in dis.disasm(pe.get_data(rva, min(end-rva, 32768)), rva):
            note = ''
            if ins.mnemonic in ('call', 'jmp') and ins.op_str.startswith('0x'):
                labels = names.get(int(ins.op_str, 16), [])
                if labels:
                    note = ' ; ' + (' / '.join(labels) if len(labels) < 7 else f'folded {len(labels)} aliases')
            output.append(f'{ins.address:X} {ins.mnemonic} {ins.op_str}{note}')
    assert len(targets) == 4, (number, targets)
    (OUTPUT / f'ff{number}_speech_entry.txt').write_text('\n'.join(output)+'\n', encoding='utf-8')
    print(f'FF{number}: native naming entry/focus call evidence saved')
