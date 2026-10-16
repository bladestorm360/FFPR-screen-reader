"""Bounded offline creation-field producer audit. No game launch, attach or calls."""
from pathlib import Path
import hashlib
import json
import re
import capstone
import pefile

ROOT = Path(__file__).resolve().parents[2]
GAMES = ['FINAL FANTASY PR', 'FINAL FANTASY II PR', 'FINAL FANTASY III PR',
         'FINAL FANTASY IV PR', 'FINAL FANTASY V PR', 'FINAL FANTASY VI PR']


def main():
    out = ROOT / 'build_check/navigation_baseline_2026_10_16'
    out.mkdir(exist_ok=True)
    receipt = []
    fields_of = {'Last.UI.KeyInput.CharacterContentController',
                 'Last.UI.KeyInput.CharacterSelectCommandContentView',
                 'Last.UI.CharacterSelectCommandData', 'Last.Defaine.UI.CharacterSelectCommnadId'}
    for n, game in enumerate(GAMES, 1):
        dumpfile = ROOT.parent / f'ff{n}/dump.cs'
        dump = dumpfile.read_text(encoding='utf-8-sig')
        functions, fields = [], {}
        for block in dump.split('// Namespace: ')[1:]:
            ns = block.splitlines()[0].strip()
            cls = re.search(r'\b(?:class|struct|enum) ([\w.<>]+)', block)
            if not cls:
                continue
            name = cls[1]
            if ns + '.' + name in fields_of:
                fields[ns + '.' + name] = [line.strip() for line in block.split('// Methods')[0].splitlines()
                                          if '; // 0x' in line or 'public const ' in line]
            for m in re.finditer(r'// RVA: 0x([A-F0-9]+)[^\n]*\n\s*((?:public|private|internal|protected) [^\n]*? ([\w.<>]+)\(([^\n]*)\))', block):
                functions.append((int(m[1], 16), ns, name, m[3], m[2]))
        boundaries = sorted({f[0] for f in functions})
        binary = Path('D:/Games/SteamLibrary/steamapps/common') / game / 'GameAssembly.dll'
        image = pefile.PE(str(binary), fast_load=True).get_memory_mapped_image()
        targets = [('Last.UI.KeyInput', 'CharacterContentController', 'SetJobName'),
                   ('Last.UI.KeyInput', 'CharacterContentController', 'SetData'),
                   ('Last.UI.KeyInput', 'CharacterContentListController', 'SelectContent'),
                   (f'Serial.FF{0 if n == 4 else n}.UI.KeyInput', 'NewGameWindowController', 'InitJobSelect')]
        methods, lines = [], []
        for target in targets:
            found = [f for f in functions if f[1:4] == target]
            if not found:
                methods.append({'target': '.'.join(target), 'absent': True})
                continue
            assert len(found) == 1
            f = found[0]
            end = next(v for v in boundaries if v > f[0])
            assert 0 < end - f[0] <= 16384
            body = image[f[0]:end]
            methods.append({'target': '.'.join(target), 'rva': hex(f[0]), 'signature': f[4],
                            'bytes': len(body), 'sha256': hashlib.sha256(body).hexdigest(),
                            'aliases': sum(x[0] == f[0] for x in functions)})
            lines.append('\n' + '.'.join(target))
            for ins in capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64).disasm(body, f[0]):
                note = ''
                if ins.mnemonic in ['call', 'jmp'] and ins.op_str.startswith('0x'):
                    names = ['.'.join(x[1:4]) for x in functions if x[0] == int(ins.op_str, 16)]
                    note = ' ; ' + (' | '.join(names) if len(names) < 4 else str(len(names)) + ' aliases')
                lines.append(f'{ins.address:X} {ins.mnemonic} {ins.op_str}{note}')
        (out / f'ff{n}_creation_native.txt').write_text('\n'.join(lines), encoding='utf-8')
        receipt.append({'game': n, 'dump_sha256': hashlib.sha256(dumpfile.read_bytes()).hexdigest(),
                        'game_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
                        'fields': fields, 'methods': methods})
    (ROOT / 'docs/re/creation_fields_native.json').write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
    print('All-six creation field/native producer hashes and bounded disassemblies preserved')


if __name__ == '__main__':
    main()
