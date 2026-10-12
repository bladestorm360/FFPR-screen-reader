"""Read-only native facing/transition evidence from the six installed matching PEs."""
import pathlib
import re
import hashlib
import capstone
import pefile

ROOT = pathlib.Path(__file__).resolve().parents[2]
STEAM = pathlib.Path('D:/Games/SteamLibrary/steamapps/common')
GAMES = ['FINAL FANTASY PR', 'FINAL FANTASY II PR', 'FINAL FANTASY III PR',
         'FINAL FANTASY IV PR', 'FINAL FANTASY V PR', 'FINAL FANTASY VI PR']
TARGETS = {
    'FieldEntity': ['LookAt', 'Look', 'To4Direction'],
    'FieldSpriteEntity': ['LookAt'],
    'GotoMapEventEntity': ['CanEnter', 'OnTriggerStayCustom'],
    'EventTriggerEntity': ['OnTriggerEnterCustom', 'OnTriggerStayCustom', 'IsTriggerEnable'],
    'FootEvent': ['PreExcute', 'Excute', 'RequestTriggerAction', 'SetupTriggerObstacles'],
}
for n, folder in enumerate(GAMES, 1):
    source = (ROOT.parent / ('ff%d' % n) / 'dump.cs').read_text(encoding='utf-8-sig')
    names, targets = {}, {}
    for block in source.split('// Namespace: ')[1:]:
        namespace = block.splitlines()[0]
        cls = re.search(r'^(?:public|private|internal).*?\b(?:class|struct) ([\w.<>]+)', block, re.M)
        if not cls:
            continue
        for entry in re.finditer(r'^\s*// RVA: 0x([0-9A-F]+)[^\n]*\n\s*(?:public|private|protected|internal) [^\n]*? ([\w.<>]+)\(([^\n]*)\)', block, re.M):
            rva, method, args = int(entry[1], 16), entry[2], entry[3]
            names.setdefault(rva, []).append('%s.%s.%s' % (namespace, cls[1], method))
            if cls[1] in TARGETS and method in TARGETS[cls[1]]:
                if method == 'LookAt' and not args.startswith('Vector2 direction,'):
                    continue
                targets[cls[1] + '.' + method] = rva
    binary = STEAM / folder / 'GameAssembly.dll'
    pe = pefile.PE(str(binary), fast_load=True)
    ordered = sorted(names)
    disassembler = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    output = ['GameAssembly SHA-256: ' + hashlib.sha256(binary.read_bytes()).hexdigest()]
    decoded = {}
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
    assert 'FieldSpriteEntity.LookAt' in targets and 'FieldEntity.LookAt' in targets
    sprite = decoded['FieldSpriteEntity.LookAt']
    assert any(ins.mnemonic == 'movq' and ins.op_str.endswith(', rdx') for ins in sprite[:16]), 'Facing Vector2 ABI changed'
    base_va = pe.OPTIONAL_HEADER.ImageBase + targets['FieldEntity.LookAt']
    calls_base = any(ins.mnemonic == 'call' and ins.op_str == hex(base_va) for ins in sprite)
    direction_writer = decoded['FieldEntity.LookAt'] if calls_base else sprite
    for offset in (0x50, 0x54):
        assert any(ins.mnemonic == 'movss' and ins.op_str.startswith('dword ptr [rbx + 0x%x],' % offset)
                   for ins in direction_writer), 'Native direction write changed'
    (ROOT / 'docs/re' / ('ff%d_arrival_calls.txt' % n)).write_text('\n'.join(line.rstrip() for line in output) + '\n', encoding='utf-8')
    (ROOT / 'docs/re' / ('ff%d_arrival_manifest.tsv' % n)).write_text(
        ''.join('%X\t%s\n' % (pe.OPTIONAL_HEADER.ImageBase + rva, name) for name, rva in targets.items()), encoding='utf-8')
    print('FF%d: facing/transition call evidence exported (%d entry points)' % (n, len(targets)))
