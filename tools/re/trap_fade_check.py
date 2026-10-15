"""Read-only, bounded all-six fade readiness and FF2 pitfall operand evidence.

Native bytes/disassembly remain in ignored build_check/re. No game is started.
"""
from pathlib import Path
from collections import defaultdict
import hashlib
import re
import struct
import capstone
import pefile

ROOT = Path(__file__).resolve().parents[2]
GAMES = ['FINAL FANTASY', 'FINAL FANTASY II', 'FINAL FANTASY III',
         'FINAL FANTASY IV', 'FINAL FANTASY V', 'FINAL FANTASY VI']
OUT = ROOT / 'build_check/re/trap_fade'
OUT.mkdir(parents=True, exist_ok=True)
for number, title in enumerate(GAMES, 1):
    dump_root = ROOT.parent / ('FF4' if number == 4 else 'FF5' if number == 5 else f'ff{number}')
    dump = dump_root / 'dump.cs'
    binary = Path('D:/Games/SteamLibrary/steamapps/common') / (title + ' PR') / 'GameAssembly.dll'
    names, targets = defaultdict(list), {}
    for block in dump.read_text(encoding='utf-8-sig').split('// Namespace: ')[1:]:
        namespace = block.splitlines()[0]
        cls = re.search(r'^(?:public|private|internal).*?\b(?:class|struct) ([\w.<>]+)', block, re.M)
        if not cls:
            continue
        for match in re.finditer(r'// RVA: 0x([0-9A-F]+)[^\n]*\n\s*(?:public|private|protected|internal) ([^\n]*? ([\w.<>]+)\([^\n]*)', block):
            rva, method = int(match[1], 16), match[3]
            label = namespace + '.' + cls[1] + '.' + method
            names[rva].append(label)
            if namespace == 'System.Fade' and cls[1] == 'FadeManager' and method in ('IsFadeFinish', 'IsStateFadeIn'):
                targets[label] = rva
            if number == 5 and namespace == 'Last.Map' and method == 'GetScriptData':
                targets[label] = rva
            if ((cls[1] == 'AxisAccessor' and method == '.ctor') or
                    (cls[1] == 'AxisAccessor.<>c' and method.startswith('<.ctor>')) or
                    (cls[1] == 'Puppet' and method in ('.cctor', 'SetPos', 'Lerp', 'MoveTo', 'Move', 'GetDirection', 'GetChangeLayerValue', 'GetDirectTargetPosition'))):
                targets[label] = rva
    assert sum('.FadeManager.' in label for label in targets) == 2, (number, targets)
    pe = pefile.PE(str(binary), fast_load=True)
    ordered = sorted(names)
    dis = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    dis.detail = True
    output = ['GameAssembly SHA256: ' + hashlib.sha256(binary.read_bytes()).hexdigest()]
    for label, rva in targets.items():
        end = next(address for address in ordered if address > rva)
        if label.endswith('Puppet..cctor'):
            end = min(end, rva + 1536)  # only the direction/layer array initializer prefix
        assert 0 < end - rva <= 8192, (label, end-rva)
        insns = list(dis.disasm(pe.get_data(rva, end-rva), rva))
        output.append(f'\n{label} RVA {rva:X}, bounded bytes {end-rva}')
        for ins in insns:
            labels = names.get(int(ins.op_str, 16), []) if ins.mnemonic in ('call', 'jmp') and ins.op_str.startswith('0x') else []
            output.append(f'{ins.address:X} {ins.mnemonic} {ins.op_str}' + (' ; ' + ' / '.join(labels[:6]) if labels else ''))
        if label.endswith('Puppet..cctor'):
            directions=[]
            for ins in insns:
                if ins.mnemonic=='movss' and len(ins.operands)==2 and ins.operands[1].type==capstone.x86.X86_OP_MEM:
                    operand=ins.operands[1]
                    if operand.mem.base==capstone.x86.X86_REG_RIP:
                        directions.append(struct.unpack('<f',pe.get_data(ins.address+ins.size+operand.mem.disp,4))[0])
            assert directions[:4]==[-1,1,1,-1], (number,directions)
            output.append('Verified grid direction components: Up Y=-1, Right X=1, Down Y=1, Left X=-1')
        if label.endswith('Puppet.Move'):
            calls=[name for i in insns if i.mnemonic=='call' and i.op_str.startswith('0x') for name in names.get(int(i.op_str,16),[])]
            assert any(name.endswith('Puppet.GetDirection') for name in calls), label
            assert any(name.endswith('Vector3.op_Multiply') for name in calls), label
            assert any(name.endswith('Vector3.op_Addition') for name in calls), label
        if label.endswith('.IsStateFadeIn'):
            # Every native implementation reads CanvasRenderer (field 0x28), gets
            # alpha, compares with floating zero, and returns that comparison.
            assert any('0x28]' in i.op_str for i in insns), label
            assert any(i.mnemonic == 'ucomiss' for i in insns), label
            assert any(i.mnemonic == 'xorps' and i.op_str == 'xmm1, xmm1' for i in insns), label
            assert any(i.mnemonic == 'mov' and i.op_str == 'al, 1' for i in insns), label
            assert any(i.mnemonic == 'xor' and i.op_str == 'al, al' for i in insns), label
            assert any(i.mnemonic == 'jne' for i in insns) and any(i.mnemonic == 'jp' for i in insns), label
            print(f'FF{number}: IsStateFadeIn RVA {rva:X} verifies actual renderer alpha-zero readiness')
    (OUT / f'ff{number}.txt').write_text('\n'.join(output) + '\n', encoding='utf-8')
