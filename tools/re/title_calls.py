"""Read-only title/cinematic call-path evidence from all six PE files and dumps.

No DLL is loaded. Disassemble only metadata-sized function ranges; retain direct
call names and complete instructions so inlined SetActive paths are inspectable.
New raw exports stay in ignored build_check/re; preserve the historical archive.
Requires the already-installed pefile and capstone packages.
"""
import pathlib
import re
import hashlib
import capstone
import pefile

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUTPUT = ROOT / 'build_check/re'
OUTPUT.mkdir(parents=True, exist_ok=True)
STEAM = pathlib.Path('D:/Games/SteamLibrary/steamapps/common')
GAMES = ['FINAL FANTASY PR', 'FINAL FANTASY II PR', 'FINAL FANTASY III PR',
         'FINAL FANTASY IV PR', 'FINAL FANTASY V PR', 'FINAL FANTASY VI PR']
METHODS = ['InitNone', 'UpdateNone', 'InitShortcutCommand', 'UpdateShortcutCommand',
           'SetEnableStartObject', 'PreloadCheck', 'ShortcutExtraCommand',
           'OnAdvertisement', 'SetTitleFadeCallback', 'FreezeStateMachine',
           'SetEnableTitleWindow']
DEMO_METHODS = ['Setup', 'Start', 'Idle', 'Boot', 'EndFade', 'TitleFadeIn',
                'Update', '<Update>b__124_0', '<Update>b__48_0', '<Update>b__48_1']

for n, folder in enumerate(GAMES, 1):
    source = (ROOT.parent / ('ff%d' % n) / 'dump.cs').read_text(encoding='utf-8-sig')
    names = {}
    targets = {}
    for block in source.split('// Namespace: ')[1:]:
        namespace = block.splitlines()[0]
        cls = re.search(r'^(?:public|private|internal).*?\b(?:class|struct) ([\w.<>]+)', block, re.M)
        if not cls:
            continue
        for entry in re.finditer(r'^\s*// RVA: 0x([0-9A-F]+)[^\n]*\n\s*(?:public|private|protected|internal) [^\n]*? ([\w.<>]+)\(', block, re.M):
            rva, name = int(entry[1], 16), entry[2]
            names.setdefault(rva, []).append('%s.%s.%s' % (namespace, cls[1], name))
            if namespace == 'Last.UI.KeyInput' and cls[1] == 'TitleWindowController' and name in METHODS:
                targets[cls[1] + '.' + name] = rva
            if namespace == 'Last.FF%d.Demo' % n and cls[1] == 'Advertisement' and name in DEMO_METHODS:
                targets[cls[1] + '.' + name] = rva
    pe = pefile.PE(str(STEAM / folder / 'GameAssembly.dll'), fast_load=True)
    ordered = sorted(names)
    disassembler = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    output = ['GameAssembly SHA256: ' + hashlib.sha256((STEAM / folder / 'GameAssembly.dll').read_bytes()).hexdigest()]
    decoded = {}
    for name, rva in targets.items():
        following = next((x for x in ordered if x > rva), rva + 4096)
        code = pe.get_data(rva, min(following - rva, 16384))
        output.append('\nFF%d %s RVA %X' % (n, name, rva))
        instructions = list(disassembler.disasm(code, pe.OPTIONAL_HEADER.ImageBase + rva))
        decoded[name] = instructions
        for instruction in instructions:
            annotation = ''
            if instruction.mnemonic in ('call', 'jmp') and instruction.op_str.startswith('0x'):
                destination = int(instruction.op_str, 16) - pe.OPTIONAL_HEADER.ImageBase
                labels = names.get(destination, [])
                annotation = ' ; ' + (' / '.join(labels) if len(labels) <= 4
                                     else 'folded body (%d metadata aliases)' % len(labels))
            output.append('%X %s %s%s' % (instruction.address, instruction.mnemonic, instruction.op_str, annotation))
    def calls(instruction, suffix):
        if instruction.mnemonic not in ('call', 'jmp') or not instruction.op_str.startswith('0x'):
            return False
        rva = int(instruction.op_str, 16) - pe.OPTIONAL_HEADER.ImageBase
        return any(label.endswith(suffix) for label in names.get(rva, []))

    # Confirm the executed producer path, not merely the existence of a wrapper.
    startup = decoded['TitleWindowController.UpdateNone']
    hide = next(i for i, ins in enumerate(startup)
                if calls(ins, 'Last.Systems.Indicator.SystemIndicator.Hide'))
    activation = next(i for i in range(hide + 1, len(startup))
                      if calls(startup[i], 'UnityEngine.SafeSetFunction.SafeActiveSet'))
    assert activation - hide <= 12, 'FF%d: prompt activation moved away from Hide' % n
    assert any(ins.mnemonic == 'mov' and ins.op_str == 'dl, 1'
               for ins in startup[hide:activation]), 'FF%d: startup does not enable prompt' % n
    assert not any(calls(ins, 'TitleWindowController.SetEnableStartObject')
                   for ins in startup), 'FF%d: startup no longer inlines the wrapper' % n
    shortcut = decoded['TitleWindowController.InitShortcutCommand']
    assert any(calls(ins, 'UnityEngine.SafeSetFunction.SafeActiveSet')
               and any(prev.mnemonic == 'mov' and prev.op_str == 'dl, 1'
                       for prev in shortcut[max(0, i - 5):i])
               for i, ins in enumerate(shortcut)), 'FF%d: shortcut does not enable prompt' % n
    destination = OUTPUT / ('ff%d_title_calls.txt' % n)
    destination.write_text('\n'.join(output) + '\n', encoding='utf-8')
    (OUTPUT / ('ff%d_title_manifest.tsv' % n)).write_text(
        ''.join('%X\t%s\n' % (pe.OPTIONAL_HEADER.ImageBase + rva, name)
                for name, rva in targets.items()), encoding='utf-8')
    print('FF%d: native startup/shortcut prompt activation verified; %d entry points' % (n, len(targets)))
