"""Offline movement ABI/call-path evidence. Reads PE bytes, never loads a game."""
import pathlib
import re
import struct
import capstone
import pefile

ROOT = pathlib.Path(__file__).resolve().parents[2]
STEAM = pathlib.Path('D:/Games/SteamLibrary/steamapps/common')
GAMES = ['FINAL FANTASY PR', 'FINAL FANTASY II PR', 'FINAL FANTASY III PR',
         'FINAL FANTASY IV PR', 'FINAL FANTASY V PR', 'FINAL FANTASY VI PR']
TARGETS = {
    'InputDevice': ['ObserveInput', 'get_InputAxis'],
    'FieldPlayerKeyController': ['OnTouchPadCallback'],
    'FieldEntity': ['get_IsMoving', 'get_IsMovingDestPosition'],
}
for n, folder in enumerate(GAMES, 1):
    source = (ROOT.parent / ('ff%d' % n) / 'dump.cs').read_text(encoding='utf-8-sig')
    names, targets = {}, {}
    for block in source.split('// Namespace: ')[1:]:
        namespace = block.splitlines()[0]
        cls = re.search(r'^(?:public|private|internal).*?\b(?:class|struct) ([\w.<>]+)', block, re.M)
        if not cls:
            continue
        for entry in re.finditer(r'^\s*// RVA: 0x([0-9A-F]+)[^\n]*\n\s*(?:public|private|protected|internal) [^\n]*? ([\w.<>]+)\(', block, re.M):
            rva, method = int(entry[1], 16), entry[2]
            names.setdefault(rva, []).append('%s.%s.%s' % (namespace, cls[1], method))
            if cls[1] in TARGETS and method in TARGETS[cls[1]]:
                targets[cls[1] + '.' + method] = rva
    pe = pefile.PE(str(STEAM / folder / 'GameAssembly.dll'), fast_load=True)
    ordered = sorted(names)
    disassembler = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    output, decoded = [], {}
    for name, rva in targets.items():
        following = next((x for x in ordered if x > rva), rva + 4096)
        output.append('\nFF%d %s RVA %X' % (n, name, rva))
        instructions = list(disassembler.disasm(pe.get_data(rva, following - rva), pe.OPTIONAL_HEADER.ImageBase + rva))
        decoded[name] = instructions
        for ins in instructions:
            annotation = ''
            if ins.mnemonic in ('call', 'jmp') and ins.op_str.startswith('0x'):
                labels = names.get(int(ins.op_str, 16) - pe.OPTIONAL_HEADER.ImageBase, [])
                annotation = ' ; ' + (' / '.join(labels) if len(labels) <= 4 else 'folded body (%d aliases)' % len(labels))
            output.append('%X %s %s%s' % (ins.address, ins.mnemonic, ins.op_str, annotation))
    assert len(targets) == 5, 'Missing movement entry point for FF%d' % n
    axis_va = pe.OPTIONAL_HEADER.ImageBase + targets['InputDevice.get_InputAxis']
    assert any(ins.mnemonic == 'call' and ins.op_str == hex(axis_va)
               for ins in decoded['InputDevice.ObserveInput']), 'Axis getter inlined for FF%d' % n
    observe = decoded['InputDevice.ObserveInput']
    axis_call = next(i for i, ins in enumerate(observe) if ins.mnemonic == 'call' and ins.op_str == hex(axis_va))
    assert any(ins.mnemonic == 'movq' and ins.op_str.endswith(', rax')
               for ins in observe[axis_call + 1:axis_call + 4]), 'Vector2 RAX return ABI changed'
    movement = decoded['FieldPlayerKeyController.OnTouchPadCallback']
    assert any(ins.mnemonic == 'movq' and ins.op_str.endswith(', rdx') for ins in movement[:15]), 'Vector2 RDX argument ABI changed'
    scales = []
    for ins in movement:
        literal = re.fullmatch(r'xmm1, dword ptr \[rip ([+-]) 0x([0-9a-f]+)\]', ins.op_str)
        if ins.mnemonic == 'movss' and literal:
            offset = int(literal[2], 16) * (1 if literal[1] == '+' else -1)
            scales.append(struct.unpack('<f', pe.get_data(ins.address + ins.size + offset - pe.OPTIONAL_HEADER.ImageBase, 4))[0])
    assert 16.0 in scales, 'Native movement displacement changed'
    (ROOT / 'docs/re' / ('ff%d_movement_calls.txt' % n)).write_text('\n'.join(line.rstrip() for line in output) + '\n', encoding='utf-8')
    print('FF%d: axis call, Vector2 ABI and native 16-unit movement verified' % n)
